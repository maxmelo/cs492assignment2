#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

using namespace std;

/* Define the total number of memory locations usable for the simulation */
#define LOCATIONS 512

/*  
    Create a class that represents a page, with member variables 
    for its index, when it was accessed, whether it's valid, and
    R (for the Clock page replacement algorithm)
*/
class Page {
    public:
    int num_page;
    unsigned long accessed;

    bool valid;
    bool R;
    
    /* Initialize values for page */
    Page(int id) {
        num_page = id;  
        accessed = 0;
        valid = false;
        R = false;
    }

    /* Load page */
    void load(unsigned long atime) {
        accessed = atime;

        valid = true;
        R = true; 
    }
};

/*
    Create a class that represents a page table. Each page table has 
    a vector of pages and indices, as well as a PID. Each of the 
    page replacement algorithms are implemented here
*/
class PageTable {
    public:
    int ind_clock;
    int mem;

    std::string pid;
    std::vector<Page*> table;
    std::vector<int> indices;

    PageTable() {
        mem = 0;
        ind_clock = 0;
    }

    //perform the FIFO page replacement algorithm
    void do_fifo(int ind) {
        table[indices.front()]->valid = 0;
        table[ind]->valid = 1;

        indices.erase(indices.begin());
        indices.push_back(ind);
    }
    //perform the LRU page replacement algorithm
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
    //perform the Clock page replacement algorithm
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
    /* Retrieve the next invalid page, wrapping around from 0 if necessary. If not found, return -1 */
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

/* Given a list of page tables and a PID, find the index of the page table with that PID */
int find_page_table(vector<PageTable*> ptlist, string pid) {
    for (int i = 0; i < ptlist.size(); i++) {
        if (ptlist[i]->pid == pid) return i;
    }
    return -1;
}

/* 
    Read in a file, line by line, and return a vector of vectors of strings.
    Each vector inside the returned vector represents a line, with each
    string within these vectors representing the items on the line, separated
    by the given separator.
*/
vector<vector <string> > read_in(string filename, char separator) {
    ifstream instream(filename);
    vector<vector <string> > out;
    string instring;

    if (!instream) throw 1;

    //Read file line-by-line
    while (getline(instream, instring)) {
        stringstream line(instring);
        string segment;
        vector<string> seglist;

        if (instring.size() == 0) continue;

        //Read line string-by-string, separated by given separator
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

    /* Check for valid page size */
    if (page_size < 1 || page_size > 32 || (page_size & (page_size - 1)) != 0) {
        cout << "Invalid page size. Please enter a page size between 1 and 32 (inclusive) that is also a power of 2" << endl;
        return 1;
    }

    /* Check for valid page replacement algorithm */
    bool algo_fifo = strcmp(algo, "FIFO") == 0;
    bool algo_lru = strcmp(algo, "LRU") == 0;
    bool algo_clock = strcmp(algo, "Clock") == 0;
    if (!algo_fifo && !algo_lru && !algo_clock) {
        cout << "Invalid page replacement algorithm. Valid algorithms are \"FIFO\", \"LRU\", or \"Clock\"" << endl;
        return 1;
    }


    /* Check for valid prepaging directive */
    if (!strcmp(argv[5], "+") && !strcmp(argv[5], "-")) {
        cout << "Prepaging input invalid" << endl;
        return 1;
    }
    prepaging = (strcmp(argv[5], "+")) ? 1 : 0;
    

    /* Read plist file and store in 2d vector */
    vector<vector <string> > plist;
    try {
        plist = read_in(plist_path, ' ');
    } catch (int e) {
        cout << "Invalid filepath for plist" << endl;
        return 1;
    }


    /* Initialize the page tables and store them in a vector */
	vector<PageTable*> page_tables;
    for (vector<vector <string> >::iterator it = plist.begin(); it != plist.end(); it++) {
        if (it->size() != 2) continue;

        PageTable *page_table = new PageTable();
        page_table->pid = it->at(0);
        page_table->mem = stoi(it->at(1));
        page_tables.push_back(page_table);
    }

    /* Fill up the page tables with new pages, calculating how many pages each one needs */
    for (int i = 0; i < page_tables.size(); i++) {
        for (int j = 0; j < ceil(page_tables[i]->mem / (page_size*1.0)); j++) {
            Page* new_page = new Page(i * page_tables.size() + j);
            page_tables[i]->table.push_back(new_page);
        }
    }

    /* Once the page tables are fully populated, loop through them again to load them all */
    for (int i = 0; i < page_tables.size(); i++) {
        for (int j = 0; j < min((LOCATIONS / page_tables.size()) / page_size, page_tables[i]->table.size()); j++) {
            page_tables[i]->table[j]->load(0);
            page_tables[i]->indices.push_back(j);
        }
    }

    /* Read ptrace file and store in a 2D vector */
    vector<vector <string> > ptrace;
    try {
        ptrace = read_in(ptrace_path, ' ');
    } catch (int e) {
        cout << "Invalid filepath for ptrace" << endl;
        return 1;
    }


    /* 
        Loop through each ptrace and perform the memory requests, performing 
        a replacement algorithm if it's not already in memory
    */
    int replacements;
    int accesses;
    for (vector<vector <string> >::iterator it = ptrace.begin(); it != ptrace.end(); it++) {
        if (it->size() != 2) continue;

        accesses++;

        //Parse lines
        string pid = it->at(0);
        int memloc = stoi(it->at(1));

        int table_index = find_page_table(page_tables, pid); //Find the page table referenced by the line
        int page = ceil(memloc / (page_size*1.0))-1; //
        
        if (page_tables[table_index]->table[page]->valid) {
            page_tables[table_index]->table[page]->load(accesses);
        } else {
            //determine whether or not to do prepaging
            int next_page = page_tables[table_index]->get_next_invalid(page);
            bool do_prepaging = prepaging && next_page != -1;
            if (do_prepaging) accesses++;

            if (algo_fifo) { //FIFO algorithm
                page_tables[table_index]->do_fifo(page);

                if (do_prepaging) page_tables[table_index]->do_fifo(next_page);
            } else if (algo_lru) { //LRU algorithm
                page_tables[table_index]->do_lru(page, accesses);
                
                if (do_prepaging) page_tables[table_index]->do_lru(next_page, accesses);
            } else if (algo_clock) { //Clock algorithm
                page_tables[table_index]->do_clock(page);

                if (do_prepaging) page_tables[table_index]->do_clock(next_page);
            }

            replacements++; //Keep a record of the replacements for printing out later
        }
    }

    printf("Number of page replacements: %d\n", replacements);
    return 0;
}