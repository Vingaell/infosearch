from __future__ import annotations

import hashlib
import json
import os
import sys
import time
from datetime import datetime
from typing import Iterable, Optional

import wikipediaapi
import requests
from urllib3.exceptions import ProtocolError


def iter_pages(categorymembers, level: int = 0, max_level: int = 3) -> Iterable[wikipediaapi.WikipediaPage]:
    for c in categorymembers.values():
        if c.ns == wikipediaapi.Namespace.CATEGORY and level < max_level:
            yield from iter_pages(c.categorymembers, level=level + 1, max_level=max_level)
        elif c.ns == wikipediaapi.Namespace.MAIN:
            yield c


def slugify_title(title: str) -> str:
    return title.replace('/', '_')


def sha1(text: str) -> str:
    return hashlib.sha1(text.encode('utf-8')).hexdigest()


def save_doc_jsonl(path: str, doc: dict):
    with open(path, 'a', encoding='utf-8') as fh:
        fh.write(json.dumps(doc, ensure_ascii=False) + '\n')


def load_processed(path: str) -> set:
    if not os.path.exists(path):
        return set()
    with open(path, 'r', encoding='utf-8') as fh:
        return set(line.strip() for line in fh if line.strip())


def append_processed(path: str, title: str):
    with open(path, 'a', encoding='utf-8') as fh:
        fh.write(title + '\n')


def parse_http_date(date_str: Optional[str]) -> Optional[int]:
    if not date_str:
        return None
    try:
        for fmt in ['%a, %d %b %Y %H:%M:%S %Z', '%a, %d %b %Y %H:%M:%S GMT']:
            try:
                dt = datetime.strptime(date_str, fmt)
                return int(dt.timestamp())
            except ValueError:
                continue
    except:
        pass
    return None


def fetch_page_with_headers(title: str, language: str, attempts: int = 3, backoff: float = 2.0):
    url = f"https://{language}.wikipedia.org/wiki/{title.replace(' ', '_')}"
    
    for i in range(1, attempts + 1):
        try:
            response = requests.get(
                url,
                headers={'User-Agent': 'InformationSearchBot/1.0'},
                timeout=10
            )
            
            if response.status_code == 200:
                last_modified = response.headers.get('Last-Modified')
                etag = response.headers.get('ETag')
                
                last_modified_ts = parse_http_date(last_modified)
                
                return {
                    'html': response.text,
                    'last_modified': last_modified,
                    'last_modified_ts': last_modified_ts,
                    'etag': etag,
                    'status_code': response.status_code
                }
            else:
                print(f"Warning: HTTP {response.status_code} for {title}", file=sys.stderr)
                return None
                
        except Exception as e:
            print(f"Warning: error fetching '{title}' (attempt {i}/{attempts}): {e}", file=sys.stderr)
            
            if i == attempts:
                return None
            
            time.sleep(backoff * i)
    
    return None


def main():
    LANGUAGE = 'ru'
    CATEGORY = 'Категория:Литература'
    OUTPUT_DIR = 'wiki_literature'
    MAX_PAGES = 35000  
    MAX_DEPTH = 5     
    RESUME = True      
    SLEEP = 0.5       
    RETRIES = 3       
    RETRY_BACKOFF = 2.0

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    jsonl_path = os.path.join(OUTPUT_DIR, 'docs.jsonl')
    processed_path = os.path.join(OUTPUT_DIR, 'processed.txt')

    wiki = wikipediaapi.Wikipedia(language=LANGUAGE,
                                  user_agent='InformationSearchBot/1.0 (contact)')

    cat = wiki.page(CATEGORY)
    if not cat.exists():
        print(f"Category not found: {CATEGORY}", file=sys.stderr)
        return 2

    processed = set()
    if RESUME:
        processed = load_processed(processed_path)

    pages_iter = iter_pages(cat.categorymembers, level=0, max_level=MAX_DEPTH)

    downloaded = 0
    total_bytes = 0
    start_time = time.time()

    print(f"Начинаем сбор статей из категории: {CATEGORY}")
    print(f"Лимит: {MAX_PAGES} статей")
    print(f"Уже собрано: {len(processed)}")
    print(f"Осталось собрать: {MAX_PAGES - len(processed)}")
    print("-" * 50)

    try:
        for page in pages_iter:
            title = page.title
            if RESUME and title in processed:
                continue
            if downloaded >= MAX_PAGES:
                break

            result = fetch_page_with_headers(title, LANGUAGE, attempts=RETRIES, backoff=RETRY_BACKOFF)
            
            if not result or not result.get('html'):
                append_processed(processed_path, title)
                print(f"Skipped: {slugify_title(title)} (no html)", file=sys.stderr)
                downloaded += 1
                continue

            html = result['html']
            
            text = wiki.page(title).text
            
            if not text:
                print(f"Warning: {slugify_title(title)} has html but no text", file=sys.stderr)

            b = len(html.encode('utf-8'))

            doc = {
                'id': sha1(title),
                'title': title,
                'url': f"https://{LANGUAGE}.wikipedia.org/wiki/{title.replace(' ', '_')}",
                'html': html,                         
                'text': text,                         
                'source': f'wikipedia:{LANGUAGE}',
                'retrieved_at': datetime.utcnow().isoformat() + 'Z',
                'last_modified': result.get('last_modified'),      
                'last_modified_ts': result.get('last_modified_ts'), 
                'etag': result.get('etag'),                         
                'bytes': b,
                'headers': {                                       
                    'last-modified': result.get('last_modified'),
                    'etag': result.get('etag')
                }
            }

            save_doc_jsonl(jsonl_path, doc)
            append_processed(processed_path, title)

            downloaded += 1
            total_bytes += b

            elapsed = time.time() - start_time
            avg_per = downloaded / elapsed if elapsed > 0 else 0
            
            status = f"[{downloaded}/{MAX_PAGES}] {slugify_title(title)}"
            if result.get('etag'):
                status += f" | ETag: {result['etag'][:20]}..."
            print(status)

            if SLEEP:
                time.sleep(SLEEP)

    except KeyboardInterrupt:
        print('\nПрервано пользователем — прогресс сохранён.')

    print(f"\nЗавершено! Скачано {downloaded} статей, всего {total_bytes/1024/1024:.2f} МБ.")
    print(f"Результаты в папке: {os.path.abspath(OUTPUT_DIR)}")


if __name__ == '__main__':
    sys.exit(main())