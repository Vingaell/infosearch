import json
import sys
import time
import hashlib
from pathlib import Path
from urllib.parse import urlparse
from datetime import datetime

import yaml
from pymongo import MongoClient, errors


def load_config(config_path="d:/python/search/robot_config.yaml"):
    if not Path(config_path).exists():
        print(f"Конфиг не найден: {config_path}")
        return None
    
    with open(config_path, 'r', encoding='utf-8') as f:
        return yaml.safe_load(f)


def normalize_url(url):
    if not url:
        return ""
    
    try:
        parsed = urlparse(url)
        normalized = f"{parsed.scheme}://{parsed.netloc}{parsed.path}".lower()
        normalized = normalized.replace('://www.', '://')
        if normalized.endswith('/'):
            normalized = normalized[:-1]
        return normalized
    except:
        return url


def iso_to_unix(iso_string):
    """ISO 8601 -> Unix timestamp"""
    if not iso_string:
        return int(datetime.now().timestamp())
    
    try:
        if iso_string.endswith('Z'):
            iso_string = iso_string.replace('Z', '+00:00')
        dt = datetime.fromisoformat(iso_string)
        return int(dt.timestamp())
    except:
        return int(datetime.now().timestamp())


def get_source_from_url(url):
    if 'wikipedia.org' in url:
        return 'wikipedia_ru'
    try:
        parsed = urlparse(url)
        return parsed.netloc.replace('www.', '')
    except:
        return 'unknown'


def main():
    print("=" * 60)
    print("ЗАГРУЗКА/ОБНОВЛЕНИЕ ДАННЫХ В MONGODB")
    print("=" * 60)
    
    config = load_config()
    if not config:
        print("\nСоздайте файл robot_config.yaml:")
        print("""
db:
  uri: "mongodb://localhost:27017/"
  database: "wikipedia_corpus"
  collection: "literature"

logic:
  jsonl_file: "wiki_literature/docs.jsonl"
  delay: 0.1
  max_documents: 35000
        """)
        return 1
    
    db_config = config.get('db', {})
    mongo_uri = db_config.get('uri', 'mongodb://localhost:27017/')
    db_name = db_config.get('database', 'wikipedia_corpus')
    collection_name = db_config.get('collection', 'literature')

    logic = config.get('logic', {})
    jsonl_path = logic.get('jsonl_file', 'wiki_literature/docs.jsonl')
    delay = logic.get('delay', 0.1)
    max_docs = logic.get('max_documents', 35000)
    
    if not Path(jsonl_path).exists():
        print(f" Файл не найден: {jsonl_path}")
        return 1
    
    try:
        client = MongoClient(mongo_uri, serverSelectionTimeoutMS=5000)
        client.admin.command('ping')
        print(f"Подключено к MongoDB")
    except errors.ConnectionFailure:
        print(f"Не удалось подключиться к MongoDB")
        print("Проверьте что MongoDB запущена")
        return 1
    
    db = client[db_name]
    collection = db[collection_name]
    
    collection.create_index('normalized_url', unique=True)
    collection.create_index('url')
    collection.create_index('crawled_at')
    collection.create_index('etag') 
    print(f"Индексы созданы/проверены")
    
    print(f"\nФайл: {jsonl_path}")
    print(f"Коллекция: {db_name}.{collection_name}")
    print(f"Лимит: {max_docs} документов")
    print(f"⏱Задержка: {delay}с")
    print("-" * 60)
    
    # Статистика
    stats = {
        'processed': 0,
        'inserted': 0,     
        'updated': 0,       
        'skipped': 0,       
        'errors': 0,
        'total_bytes': 0
    }
    
    try:
        with open(jsonl_path, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                if stats['processed'] >= max_docs:
                    print(f"\nДостигнут лимит {max_docs} документов")
                    break
                
                if not line.strip():
                    continue
                
                stats['processed'] += 1
                
                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    print(f"  Ошибка JSON на строке {line_num}")
                    stats['errors'] += 1
                    continue
                
                original_url = data.get('url') or ''
                if not original_url and data.get('title'):
                    title = data.get('title', '')
                    title_escaped = title.replace(' ', '_')
                    original_url = f"https://ru.wikipedia.org/wiki/{title_escaped}"
                
                if not original_url:
                    stats['errors'] += 1
                    continue
                

                norm_url = normalize_url(original_url)
                

                retrieved_at = data.get('retrieved_at') or data.get('time') or ''
                unix_time = iso_to_unix(retrieved_at)
                

                source = data.get('source') or get_source_from_url(original_url)
                
                html_content = data.get('html') or data.get('raw_html') or ''
                
                etag = data.get('etag')
                last_modified = data.get('last_modified_ts') or data.get('last_modified')
                
                existing = collection.find_one({'normalized_url': norm_url})
                
                mongo_doc = {
                    'url': original_url,
                    'normalized_url': norm_url,
                    'raw_html': html_content,
                    'source': source,
                    'crawled_at': unix_time,
                    'title': data.get('title', ''),
                    'imported_at': datetime.now(),
                    'etag': etag,
                    'last_modified': last_modified,
                    'doc_id': data.get('id') or hashlib.md5(original_url.encode()).hexdigest()
                }
                
                if existing:
                    old_etag = existing.get('etag')
                    old_modified = existing.get('last_modified')
                    
                    changed = False
                    if etag and old_etag and etag != old_etag:
                        changed = True
                    elif last_modified and old_modified and last_modified != old_modified:
                        changed = True
                    elif not etag and not last_modified:
                        old_html = existing.get('raw_html', '')
                        if len(html_content) != len(old_html):
                            changed = True
                    
                    if changed:
                        collection.update_one(
                            {'normalized_url': norm_url},
                            {'$set': mongo_doc}
                        )
                        stats['updated'] += 1
                        action = "ОБНОВЛЕНО"
                    else:
                        stats['skipped'] += 1
                        action = "⏭ПРОПУЩЕНО"
                else:
                    try:
                        collection.insert_one(mongo_doc)
                        stats['inserted'] += 1
                        action = "✅ ДОБАВЛЕНО"
                    except errors.DuplicateKeyError:
                        stats['errors'] += 1
                        action = "⚠️  ДУБЛИКАТ"
                
                if action != "⏭️  ПРОПУЩЕНО":
                    stats['total_bytes'] += len(html_content.encode('utf-8'))
                
                if stats['processed'] % 100 == 0:
                    print(f"  [{stats['processed']}] Добавлено: {stats['inserted']}, Обновлено: {stats['updated']}, Пропущено: {stats['skipped']}")
                
                time.sleep(delay)
        
        print("-" * 60)
        print("СТАТИСТИКА:")
        print(f"  Обработано строк: {stats['processed']}")
        print(f"  Новых документов: {stats['inserted']}")
        print(f"  Обновлено: {stats['updated']}")
        print(f"  Без изменений: {stats['skipped']}")
        print(f"  Ошибок: {stats['errors']}")
        print(f"  Всего текста: {stats['total_bytes'] / 1024 / 1024:.2f} МБ")
        
    except KeyboardInterrupt:
        print("\n\nПрервано пользователем")
    
    # Итог
    total = collection.count_documents({})
    print(f"\nГотово. Всего документов в коллекции: {total}")
    print(f"   Из них новых за этот запуск: {stats['inserted']}")
    print(f"   Обновлено: {stats['updated']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())