#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <chrono>
#include <iomanip>
#include <sstream>

using namespace std;
namespace fs = std::filesystem;

struct TokenInfo {
    size_t count;
    size_t first_seen;
};

vector<string> tokenize(const string& text) {
    static regex pattern(R"([а-яёa-z0-9]+(?:-[а-яёa-z0-9]+)*)", regex::icase);
    vector<string> tokens;
    auto begin = sregex_iterator(text.begin(), text.end(), pattern);
    auto end = sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        string t = it->str();
        transform(t.begin(), t.end(), t.begin(), ::tolower);
        tokens.push_back(t);
    }
    return tokens;
}

string format_double(double value, int precision) {
    stringstream ss;
    ss << fixed << setprecision(precision) << value;
    return ss.str();
}

int main() {
    fs::path corpus_dir = "../literature_corpus";

    if (!fs::exists(corpus_dir)) {
        cout << "❌ Папка " << corpus_dir << " не найдена\n";
        return 1;
    }

    cout << string(60, '=') << "\n";
    cout << "ТОКЕНИЗАЦИЯ КОРПУСА ДОКУМЕНТОВ\n";
    cout << string(60, '=') << "\n";
    cout << "Корпус: " << fs::absolute(corpus_dir) << "\n";

    size_t tokens_total = 0;
    size_t token_length_sum = 0;
    size_t documents = 0;
    size_t bytes_total = 0;

    unordered_map<string, TokenInfo> frequencies;
    vector<string> insertion_order;
    vector<tuple<string,string,size_t>> docs_by_tokens;

    size_t min_tokens_doc = numeric_limits<size_t>::max();
    size_t max_tokens_doc = 0;
    string min_id, min_title, min_text;
    string max_id, max_title, max_text;

    vector<fs::path> tsv_files;
    for (auto& p : fs::directory_iterator(corpus_dir)) {
        if (p.path().filename().string().rfind("part_",0)==0 && p.path().extension()==".tsv")
            tsv_files.push_back(p.path());
    }
    sort(tsv_files.begin(), tsv_files.end());

    cout << "Найдено файлов: " << tsv_files.size() << "\n";

    auto start = chrono::high_resolution_clock::now();

    for (size_t i=0;i<tsv_files.size();++i) {
        cout << "\rОбработка: [" << i+1 << "/" << tsv_files.size() << "]";
        cout.flush();

        ifstream file(tsv_files[i]);
        string line;
        getline(file,line);

        while (getline(file,line)) {
            stringstream ss(line);
            string doc_id,title,text;
            getline(ss,doc_id,'\t');
            getline(ss,title,'\t');
            getline(ss,text);

            if (text.empty()) continue;

            auto tokens = tokenize(text);
            size_t count = tokens.size();

            documents++;
            tokens_total += count;
            bytes_total += text.size();
            docs_by_tokens.push_back({doc_id,title,count});

            for (auto& t : tokens) {
                auto it = frequencies.find(t);
                if (it == frequencies.end()) {
                    frequencies[t] = {1, insertion_order.size()};
                    insertion_order.push_back(t);
                } else {
                    it->second.count++;
                }
                token_length_sum += t.size();
            }

            if (count>0 && count<min_tokens_doc) {
                min_tokens_doc=count;
                min_id=doc_id;
                min_title=title;
                min_text=text.substr(0,300);
                if (text.size()>300) min_text+="...";
            }

            if (count>max_tokens_doc) {
                max_tokens_doc=count;
                max_id=doc_id;
                max_title=title;
                max_text=text.substr(0,300);
                if (text.size()>300) max_text+="...";
            }
        }
    }

    cout << "\n";

    auto end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(end-start).count();

    size_t tokens_unique = frequencies.size();
    double avg_token_len = tokens_total ? (double)token_length_sum/tokens_total : 0;
    double avg_doc_tokens = documents ? (double)tokens_total/documents : 0;

    vector<size_t> counts;
    for (auto& d : docs_by_tokens)
        counts.push_back(get<2>(d));

    sort(counts.begin(),counts.end());
    size_t median = counts.empty()?0:counts[counts.size()/2];
    size_t min_tokens = counts.empty()?0:counts.front();
    size_t max_tokens = counts.empty()?0:counts.back();

    double kb = bytes_total/1024.0;
    double speed_kb = elapsed?kb/elapsed:0;
    double speed_tok = elapsed?tokens_total/elapsed:0;

    vector<pair<string,TokenInfo>> freq_vec(frequencies.begin(),frequencies.end());
    sort(freq_vec.begin(),freq_vec.end(),
         [](auto&a,auto&b){
            if(a.second.count!=b.second.count)
                return a.second.count>b.second.count;
            return a.second.first_seen<b.second.first_seen;
         });

    vector<string> rare_words;
    for (auto& p : freq_vec)
        if (p.second.count==1)
            rare_words.push_back(p.first);

    cout << string(60,'=') << "\n";
    cout << "РЕЗУЛЬТАТЫ ТОКЕНИЗАЦИИ\n";
    cout << string(60,'=') << "\n";

    cout << "\nОБЩАЯ СТАТИСТИКА:\n";
    cout << "  Документов обработано: " << documents << "\n";
    cout << "  Объём текста: " << format_double(bytes_total/(1024.0*1024.0),2) << " МБ\n";

    cout << "\nТОКЕНЫ:\n";
    cout << "  Всего токенов: " << tokens_total << "\n";
    cout << "  Уникальных токенов: " << tokens_unique << "\n";
    cout << "  Средняя длина токена: " << format_double(avg_token_len,2) << " символов\n";

    cout << "\nДОКУМЕНТЫ:\n";
    cout << "  Средняя длина документа: " << (size_t)avg_doc_tokens << " токенов\n";
    cout << "  Медианная длина документа: " << median << " токенов\n";
    cout << "  Минимальная длина: " << min_tokens << " токенов\n";
    cout << "  Максимальная длина: " << max_tokens << " токенов\n";

    cout << "\nПРОИЗВОДИТЕЛЬНОСТЬ:\n";
    cout << "  Время выполнения: " << format_double(elapsed,2) << " секунд\n";
    cout << "  Скорость: " << format_double(speed_kb,2) << " КБ/сек\n";
    cout << "  Скорость: " << (size_t)speed_tok << " токенов/сек\n";

    cout << "\n🏆 ТОП-20 САМЫХ ЧАСТЫХ ТОКЕНОВ:\n";
    for (size_t i=0;i<min((size_t)20,freq_vec.size());++i)
        cout << setw(3)<<i+1<<". "<<setw(20)<<left<<freq_vec[i].first
             <<" - "<<freq_vec[i].second.count<<" раз\n";

    cout << "\nПРИМЕРЫ РЕДКИХ ТОКЕНОВ (встретились 1 раз):\n  ";
    for (size_t i=0;i<min((size_t)10,rare_words.size());++i){
        if(i) cout<<", ";
        cout<<rare_words[i];
    }
    cout<<"\n";

    cout << "\n📏 САМЫЙ МАЛЕНЬКИЙ ДОКУМЕНТ:\n";
    cout << "  Название: " << min_title << "\n";
    cout << "  Токенов: " << min_tokens_doc << "\n";
    cout << "  Текст: " << min_text << "\n";

    cout << "\n📏 САМЫЙ БОЛЬШОЙ ДОКУМЕНТ:\n";
    cout << "  Название: " << max_title << "\n";
    cout << "  Токенов: " << max_tokens_doc << "\n";
    cout << "  Текст: " << max_text << "\n";

    ofstream out(corpus_dir/"tokenization_stats.txt");
    out<<"СТАТИСТИКА ТОКЕНИЗАЦИИ\n"<<string(50,'=')<<"\n\n";
    out<<"ОБЩАЯ СТАТИСТИКА:\n";
    out<<"  Документов: "<<documents<<"\n";
    out<<"  Объём текста: "<<format_double(bytes_total/(1024.0*1024.0),2)<<" МБ\n";
    out<<"  Объём текста: "<<format_double(kb,2)<<" КБ\n\n";

    out<<"ТОКЕНЫ:\n";
    out<<"  Всего токенов: "<<tokens_total<<"\n";
    out<<"  Уникальных токенов: "<<tokens_unique<<"\n";
    out<<"  Средняя длина токена: "<<format_double(avg_token_len,2)<<"\n\n";

    out<<"ДОКУМЕНТЫ:\n";
    out<<"  Средняя длина документа: "<<(size_t)avg_doc_tokens<<" токенов\n";
    out<<"  Медианная длина документа: "<<median<<" токенов\n";
    out<<"  Минимальная длина: "<<min_tokens<<" токенов\n";
    out<<"  Максимальная длина: "<<max_tokens<<" токенов\n\n";

    out<<"ПРОИЗВОДИТЕЛЬНОСТЬ:\n";
    out<<"  Время выполнения: "<<format_double(elapsed,2)<<" сек\n";
    out<<"  Скорость: "<<format_double(speed_kb,2)<<" КБ/сек\n";
    out<<"  Скорость: "<<(size_t)speed_tok<<" токенов/сек\n\n";

    out<<"ТОП-20 САМЫХ ЧАСТЫХ ТОКЕНОВ:\n";
    for(size_t i=0;i<min((size_t)20,freq_vec.size());++i)
        out<<"  "<<freq_vec[i].first<<": "<<freq_vec[i].second.count<<"\n";

    out<<"\nПРИМЕРЫ РЕДКИХ ТОКЕНОВ (первые 20):\n  ";
    for(size_t i=0;i<min((size_t)20,rare_words.size());++i){
        if(i) out<<", ";
        out<<rare_words[i];
    }
    out<<"\n\nСАМЫЙ МАЛЕНЬКИЙ ДОКУМЕНТ:\n";
    out<<"  Название: "<<min_title<<"\n";
    out<<"  Токенов: "<<min_tokens_doc<<"\n";
    out<<"  Текст: "<<min_text<<"\n";

    out<<"\nСАМЫЙ БОЛЬШОЙ ДОКУМЕНТ:\n";
    out<<"  Название: "<<max_title<<"\n";
    out<<"  Токенов: "<<max_tokens_doc<<"\n";
    out<<"  Текст: "<<max_text<<"\n";

    out.close();

    cout<<"\nПолная статистика сохранена в "<<(corpus_dir/"tokenization_stats.txt")<<"\n";
    return 0;
}