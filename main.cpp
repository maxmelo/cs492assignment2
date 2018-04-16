#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <math.h>

using namespace std;

#define LOCATIONS 512

class Page {
    public:
    int num_page;
    unsigned long accessed;

    bool valid;
    bool R;

    Page(int id) {
        num_page = id;  
        accessed = 0;
        valid = false;
        R = false;
    }

    void load(unsigned long atime) {
        valid = true;
        R = true; 
        accessed = atime;
    }
};

class PageTable {
    public:
    int ind_clock;
    int mem;

    std::string processID;
    std::vector<Page*> table;
    std::vector<int> indices;

    PageTable() {
        mem = 0;
        ind_clock = 0;
    }

    void do_fifo(int ind) {
        table[indices.front()]->valid = 0;
        table[ind]->valid = 1;

        indices.erase(indices.begin());
        indices.push_back(ind);
    }

    void do_lru(int ind, unsigned long atime) {
        unsigned long min = -1;
        int min_val = -1;
        
        for (int i = 0; i < indices.size(); i++) {
            if (table[indices[i]]->accessed < min) {
                min = table[indices[i]]->accessed;
                min_val = i;
            }
        }
        
        table[indices[min_val]]->valid = 0;
        table[ind]->valid = 1;
        table[ind]->accessed = atime;
        indices[min_val] = ind;
    }

    void do_clock(int ind) {
        while (table[indices[ind_clock]]->R == 1) {
            table[indices[ind_clock]]->R = 0;
            if (ind_clock + 1 == indices.size()) ind_clock = 0;
            else ind_clock++;
        }

        table[indices[ind_clock]]->valid = 0;
        table[ind]->R = 1;
        table[ind]->valid = 1;
        indices[ind_clock] = ind;

        if (ind_clock + 1 == indices.size()) ind_clock = 0;
        else ind_clock++;
    }

    int get_next_invalid(int ind) {
        for (int i = ind+1; i < table.size(); i++) {
            if (!(table[i]->valid)) return i;
        }
        for (int i = 0; i < ind; i++) {
            if (!(table[i]->valid)) return i;
        }
        return -1;
    }
};

int find_page_table(vector<PageTable*> ptlist, string pid) {
    for (int i = 0; i < ptlist.size(); i++) {
        if (ptlist[i]->processID == pid) return i;
    }
    return -1;
}

vector<vector <string> > read_in(string filename, char separator) {
    ifstream instream(filename);
    vector<vector <string> > out;
    string instring;

    if (!instream) throw 1;

    while (getline(instream, instring)) {
        stringstream line(instring);
        string segment;
        vector<string> seglist;

        if (instring.size() == 0) continue;

        while(getline(line, segment, separator)) seglist.push_back(segment);

        out.push_back(seglist);
    }

    return out;
}

int main(int argc, char **argv) {
    char* plist_path;
    char* ptrace_path;
    int page_size;
    char* algo;
    bool prepaging;

    if (argc < 6) {
        cout << "Wrong number of arguments specified" << endl;
        return 1;
    }

    plist_path = argv[1];
    ptrace_path = argv[2];
    page_size = atoi(argv[3]);
    algo = argv[4];


    if (page_size < 1 || page_size > 32 || (page_size & (page_size - 1)) != 0) {
        cout << "Invalid page size. Please enter a page size between 1 and 32 (inclusive) that is also a power of 2" << endl;
        return 1;
    }

    bool algo_fifo = strcmp(algo, "FIFO") == 0;
    bool algo_lru = strcmp(algo, "LRU") == 0;
    bool algo_clock = strcmp(algo, "Clock") == 0;

    if (!algo_fifo && !algo_lru && !algo_clock) {
        cout << "Invalid page replacement algorithm. Valid algorithms are \"FIFO\", \"LRU\", or \"Clock\"" << endl;
        return 1;
    }

    if (!strcmp(argv[5], "+") && !strcmp(argv[5], "-")) {
        cout << "Prepaging input invalid" << endl;
        return 1;
    }

    prepaging = (strcmp(argv[5], "+")) ? 1 : 0;
    
	vector<PageTable*> page_tables;
    vector<vector <string> > plist;
    
    try {
        plist = read_in(plist_path, ' ');
    } catch (int e) {
        cout << "Invalid filepath for plist" << endl;
        return 1;
    }

    /* Initialize the page tables and store them in a vector */
    for (vector<vector <string> >::iterator it = plist.begin(); it != plist.end(); it++) {
        if (it->size() != 2) continue;

        PageTable *page_table = new PageTable();
        page_table->processID = it->at(0);
        page_table->mem = stoi(it->at(1));
        page_tables.push_back(page_table);
    }

    for (int i = 0; i < page_tables.size(); i++) {
        for (int j = 0; j < ceil(page_tables[i]->mem / (page_size*1.0)); j++) {
            Page* new_page = new Page(i * page_tables.size() + j);
            page_tables[i]->table.push_back(new_page);
        }
    }

    for (int i = 0; i < page_tables.size(); i++) {
        for (int j = 0; j < min((LOCATIONS / page_tables.size()) / page_size, page_tables[i]->table.size()); j++) {
            page_tables[i]->table[j]->load(0);
            page_tables[i]->indices.push_back(j);
        }
    }

    vector<vector <string> > ptrace;
    
    try {
        ptrace = read_in(ptrace_path, ' ');
    } catch (int e) {
        cout << "Invalid filepath for ptrace" << endl;
        return 1;
    }

    int replacements;
    int accesses;
    for (vector<vector <string> >::iterator it = ptrace.begin(); it != ptrace.end(); it++) {
        if (it->size() != 2) continue;

        accesses++;

        string pid = it->at(0);
        int memloc = stoi(it->at(1));
        int table_index = find_page_table(page_tables, pid);
        int page = ceil(memloc / (page_size*1.0))-1;
        
        if (page_tables[table_index]->table[page]->valid) {
            page_tables[table_index]->table[page]->load(accesses);
        } else {
            int next_page = page_tables[table_index]->get_next_invalid(page);
            bool do_prepaging = prepaging && next_page != -1;
            if (do_prepaging) accesses++;

            if (algo_fifo) {
                page_tables[table_index]->do_fifo(page);

                if (do_prepaging) page_tables[table_index]->do_fifo(next_page);
            } else if (algo_lru) {
                page_tables[table_index]->do_lru(page, accesses);
                
                if (do_prepaging) page_tables[table_index]->do_lru(next_page, accesses);
            } else if (algo_clock) {
                page_tables[table_index]->do_clock(page);

                if (do_prepaging) page_tables[table_index]->do_clock(next_page);
            }

            replacements++;
        }
    }

    printf("Number of page replacements: %d\n", replacements);
    return 0;
}