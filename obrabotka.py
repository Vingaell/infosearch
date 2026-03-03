
import json
import os
import re
import sys
import unicodedata
from pathlib import Path

def clean_wiki_text(raw_text):
    if not raw_text:
        return ""
    
    text = unicodedata.normalize('NFC', raw_text)
    
    special_spaces = {
        '\u00A0': ' ',  # неразрывный пробел
        '\u202F': ' ',  # узкий неразрывный пробел
        '\uFEFF': '',   # BOM
        '\u00AD': '',   # мягкий перенос
        '\u200B': '',   # нулевой ширины
        '\u200C': '',
        '\u200D': ''
    }
    for char, replacement in special_spaces.items():
        text = text.replace(char, replacement)
    
    dashes = r'[\u2010\u2011\u2012\u2013\u2014\u2015\u2212]'
    text = re.sub(dashes, '-', text)
    
    text = ''.join(ch for ch in text if unicodedata.category(ch) != 'Cc' or ch in '\n\r')
    
    text = re.sub(r'(\w)\s*[-]\s*[\r\n]+\s*(\w)', r'\1\2', text, flags=re.UNICODE)
    
    text = text.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ')
    
    text = ' '.join(text.split())
    
    return text.strip()

def load_processed_ids(tsv_dir):
    seen = set()
    tsv_dir = Path(tsv_dir)
    if not tsv_dir.exists():
        return seen
    
    for tsv_file in tsv_dir.glob('part_*.tsv'):
        try:
            with open(tsv_file, 'r', encoding='utf-8') as f:
                next(f, None)
                for line in f:
                    if line.strip():
                        doc_id = line.split('\t', 1)[0]
                        seen.add(doc_id)
        except Exception as e:
            print(f"Ошибка при чтении {tsv_file}: {e}")
    
    return seen

def main():
    SOURCE_FILE = "wiki_literature/docs.jsonl"
    TARGET_DIR = "../literature_corpus"  
    DOCS_PER_FILE = 1000
    MAX_DOCS = 35000  
    
    if not os.path.exists(SOURCE_FILE):
        print(f"Ошибка: файл {SOURCE_FILE} не найден")
        print(f"Текущая директория: {os.getcwd()}")
        print("Убедитесь, что скрипт запускается из правильной папки")
        return 1
    
    target_path = Path(TARGET_DIR)
    target_path.mkdir(parents=True, exist_ok=True)
    
    existing_ids = load_processed_ids(target_path)
    print(f"Найдено существующих документов: {len(existing_ids)}")
    
    stats = {
        'total_lines': 0,
        'unique_docs': len(existing_ids),
        'raw_bytes': 0,
        'text_bytes': 0,
        'skipped_no_id': 0,
        'skipped_duplicate': 0,
        'skipped_short': 0,
        'file_number': 0
    }
    
    log_file = target_path / 'processing.log'
    log_handle = open(log_file, 'w', encoding='utf-8')
    
    current_file = None
    
    print(f"Обработка {SOURCE_FILE}...")
    print(f"Цель: до {MAX_DOCS} документов")
    
    try:
        with open(SOURCE_FILE, 'r', encoding='utf-8') as src:
            for line_number, line in enumerate(src, 1):
                if not line.strip():
                    continue
                
                if stats['unique_docs'] >= MAX_DOCS:
                    print(f"\nДостигнут лимит в {MAX_DOCS} документов")
                    break
                
                stats['total_lines'] += 1
                stats['raw_bytes'] += len(line.encode('utf-8'))
                
                try:
                    doc = json.loads(line)
                except json.JSONDecodeError:
                    log_handle.write(f"Ошибка парсинга JSON на строке {line_number}\n")
                    continue
                
                doc_id = doc.get('id', '')
                if not doc_id:
                    stats['skipped_no_id'] += 1
                    log_handle.write(f"Пропуск: отсутствует ID на строке {line_number}\n")
                    continue
                
                if doc_id in existing_ids:
                    stats['skipped_duplicate'] += 1
                    log_handle.write(f"Дубликат: {doc_id} - {doc.get('title', '')}\n")
                    continue
                
                raw_text = doc.get('text', '')
                clean_text = clean_wiki_text(raw_text)
                
                if len(clean_text) < 200:
                    stats['skipped_short'] += 1
                    log_handle.write(f"Слишком короткая: {doc_id} - {doc.get('title', '')} ({len(clean_text)} символов)\n")
                    continue
                
                title = clean_wiki_text(doc.get('title', ''))
                
                if stats['unique_docs'] % DOCS_PER_FILE == 0:
                    if current_file:
                        current_file.close()
                    
                    stats['file_number'] += 1
                    filename = target_path / f"part_{stats['file_number']:03d}.tsv"
                    current_file = open(filename, 'w', encoding='utf-8')
                    current_file.write("id\ttitle\ttext\n")
                    print(f"Создан файл: {filename}")
                
                current_file.write(f"{doc_id}\t{title}\t{clean_text}\n")
                
                existing_ids.add(doc_id)
                stats['unique_docs'] += 1
                stats['text_bytes'] += len(clean_text.encode('utf-8'))
                
                if stats['total_lines'] % 5000 == 0:
                    print(f"  Обработано строк: {stats['total_lines']}, уникальных: {stats['unique_docs']}")
        
        if current_file:
            current_file.close()
        log_handle.close()
        
    except KeyboardInterrupt:
        print("\nПрерывание пользователем")
        if current_file:
            current_file.close()
        log_handle.close()
    except Exception as e:
        print(f"Ошибка: {e}")
        if current_file:
            current_file.close()
        log_handle.close()
        return 1
    
    avg_doc_size = stats['text_bytes'] / stats['unique_docs'] if stats['unique_docs'] > 0 else 0
    
    info_file = target_path / 'corpus_info.txt'
    with open(info_file, 'w', encoding='utf-8') as f:
        f.write("ИНФОРМАЦИЯ О КОРПУСЕ\n")
        f.write("=" * 50 + "\n\n")
        f.write("ИСТОЧНИКИ:\n")
        f.write("1. https://ru.wikipedia.org/wiki/Категория:Литература\n")
        f.write("2. Подкатегории (глубина обхода: 3 уровня)\n\n")
        
        f.write("СТАТИСТИКА:\n")
        f.write(f"  Всего обработано строк: {stats['total_lines']}\n")
        f.write(f"  Уникальных документов сохранено: {stats['unique_docs']}\n")
        f.write(f"  Пропущено (нет ID): {stats['skipped_no_id']}\n")
        f.write(f"  Пропущено (дубликаты): {stats['skipped_duplicate']}\n")
        f.write(f"  Пропущено (слишком короткие): {stats['skipped_short']}\n\n")
        
        f.write("РАЗМЕРЫ:\n")
        f.write(f"  Сырые данные: {stats['raw_bytes'] / (1024*1024):.2f} МБ\n")
        f.write(f"  Очищенный текст: {stats['text_bytes'] / (1024*1024):.2f} МБ\n")
        f.write(f"  Средний размер документа: {avg_doc_size / 1024:.2f} КБ\n\n")
        
        f.write("ФАЙЛЫ:\n")
        f.write(f"  Количество TSV-файлов: {stats['file_number']}\n")
        f.write(f"  Документов в файле: до {DOCS_PER_FILE}\n")
        f.write(f"  Формат: TSV (id, title, text)\n")
        f.write(f"  Кодировка: UTF-8\n\n")
        
        f.write(f"ДАТА СОЗДАНИЯ: {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
    
    print("\n" + "=" * 50)
    print("ЗАВЕРШЕНО")
    print("=" * 50)
    print(f"Уникальных документов: {stats['unique_docs']}")
    print(f"TSV-файлов создано: {stats['file_number']}")
    print(f"Размер текста: {stats['text_bytes'] / (1024*1024):.2f} МБ")
    print(f"Результат в папке: {target_path.absolute()}")
    
    first_file = target_path / "part_001.tsv"
    if first_file.exists():
        try:
            with open(first_file, 'r', encoding='utf-8') as f:
                f.readline()  
                sample = f.readline()
                if sample:
                    parts = sample.strip().split('\t')
                    print("\nПример первого документа:")
                    print(f"  ID: {parts[0]}")
                    print(f"  Заголовок: {parts[1]}")
                    print(f"  Текст: {parts[2][:100]}...")
        except:
            pass
    
    return 0

if __name__ == "__main__":
    sys.exit(main())