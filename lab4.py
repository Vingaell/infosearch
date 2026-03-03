
import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
from collections import Counter
from scipy import stats

def main():
    corpus_dir = Path("../literature_corpus")
    
    if not corpus_dir.exists():
        print("Папка не найдена")
        return 1
    
    freqs = Counter()
    tsv_files = list(corpus_dir.glob("part_*.tsv"))
    
    print("Обработка файлов...")
    for tsv_file in tsv_files:
        with open(tsv_file, 'r', encoding='utf-8') as f:
            f.readline()  
            for line in f:
                parts = line.strip().split('\t')
                if len(parts) >= 3:
                    text = parts[2].lower()
                    words = text.split()
                    freqs.update(words)
    
    sorted_freqs = sorted(freqs.values(), reverse=True)
    
    top_freqs = sorted_freqs[:100000]
    ranks = np.array(list(range(1, len(top_freqs) + 1)))
    freqs_array = np.array(top_freqs)
    
    log_ranks = np.log(ranks)
    log_freqs = np.log(freqs_array)
    
    slope, intercept, r_value, p_value, std_err = stats.linregress(log_ranks, log_freqs)
    r_squared = r_value ** 2
    
    plt.figure(figsize=(12, 8))
    
    plt.loglog(ranks, freqs_array, 'b.', markersize=3, alpha=0.5, label='Реальные данные')
    
    C = freqs_array[0]
    zipf_line = C / ranks
    plt.loglog(ranks, zipf_line, 'r-', linewidth=2, label='Закон Ципфа (freq = C/rank)')
    
    plt.title('Закон Ципфа', fontsize=14)
    plt.xlabel('Ранг слова (log)')
    plt.ylabel('Частота слова (log)')
    plt.grid(True, alpha=0.3, which='both')
    plt.legend()
    
    text_str = f'Наклон регрессии: {slope:.3f}\nR² = {r_squared:.4f}'
    plt.text(0.7, 0.9, text_str, transform=plt.gca().transAxes,
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    plt.savefig('zipf_plot.png', dpi=150)
    print(f"\n📊 Результаты регрессии:")
    print(f"  Наклон: {slope:.3f}")
    print(f"  R² = {r_squared:.4f}")
    print(f"\n✅ График сохранён в zipf_plot.png")
    plt.show()

if __name__ == "__main__":
    sys.exit(main())