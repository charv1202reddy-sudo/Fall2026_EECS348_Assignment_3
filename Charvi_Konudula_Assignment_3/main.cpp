/*
 * ============================================================================
 * PROLOGUE COMMENTS
 * ============================================================================
 * Program name : EECS 348 Assignment 3 - CEO Email Priority Queue (main.cpp)
 *
 * Description  : C++ object-oriented program that prioritizes emails for a
 *                busy CEO. Emails are stored in a MaxHeap (priority queue)
 *                that I implemented from scratch on top of a std::vector
 *                (list-based implementation; no pre-existing heap modules
 *                such as std::priority_queue or std::make_heap are used).
 *                Priority order: Boss > Subordinate > Peer > ImportantPerson
 *                > OtherPerson. Within the same sender category, the NEWEST
 *                email is read first. If category AND date are identical, the
 *                email that arrived first in the file is read first.
 *
 * Inputs       : One command-line argument: the path to a test file.
 *                  ./email_priority <test_file>
 *                Each line of the file is one command:
 *                  EMAIL <sender category>,<subject line>,<MM-DD-YYYY>
 *                  NEXT   (show the highest-priority email, do not remove it)
 *                  READ   (remove the highest-priority email, no output)
 *                  COUNT  (show the number of unread emails)
 *
 * Output       : Terminal (stdout) only. Examples:
 *                  There are 6 emails to read.
 *                  Next email:
 *                  Sender: Boss
 *                  Subject: Never Mind
 *                  Date: 01-03-2025
 *                NEXT or READ on an empty inbox prints nothing and does not
 *                crash. Malformed lines are skipped with a note on stderr.
 *
 * Collaborators: None. No human collaborators.
 *
 * Other sources: Gemini (Google GenAI) - generated the original code that is
 *                the basis of this program (accessed through the Gemini web
 *                interface in a browser). ChatGPT (OpenAI GenAI) - generated
 *                a second version that I analyzed and compared but did NOT use
 *                as the basis (accessed through the ChatGPT web interface).
 *
 * Author       : Charvi Reddy Konudula
 *
 * Creation date: 10-01-2026 (date I first created this file / wrote this
 *                comment)
 *
 * Revision date: 10-01-2026
 *
 * Revisions    : 1) Started from Gemini's generated code.
 *                2) Added enum class SenderCategory instead of magic numbers.
 *                3) Added arrival-sequence tie-breaker (deterministic order).
 *                4) Added input validation (bad lines skipped, no crashes).
 *                5) Added trimming of '\r' / whitespace (Windows line ends).
 *                6) Moved command handling out of main() into a
 *                   CommandProcessor class (objects, not functions).
 *                7) Replaced a pointer/unchecked access with a safe design,
 *                   removed unused <sstream>, added const-correctness.
 *                8) Added prologue and a comment on every line.
 *
 * Block labels : Each block below states where the code came from:
 *                [GEMINI]          = Gemini's code, kept as generated
 *                [GEMINI-MODIFIED] = Gemini's original code, modified for
 *                                    this assignment
 *                [ADDED]           = new code added for this assignment
 *                (All code was reviewed, compiled, and tested by me on the
 *                EECS Cycle server.)
 * ============================================================================
 */

#include <iostream>   // std::cout, std::cerr for terminal output
#include <fstream>    // std::ifstream to read the test file
#include <string>     // std::string for text fields
#include <vector>     // std::vector is the list-based storage of the heap
#include <utility>    // std::swap and std::move
#include <stdexcept>  // std::invalid_argument for bad input lines
#include <cstddef>    // std::size_t for indexes and sizes

// ----------------------------------------------------------------------------
// [ADDED] Named priority levels replace Gemini's magic numbers 5,4,3,2,1.
// A larger number means the sender is read EARLIER.
// ----------------------------------------------------------------------------
enum class SenderCategory {   // scoped enum so names don't leak globally
    OtherPerson = 1,          // lowest priority, read last
    ImportantPerson = 2,      // read after Peer
    Peer = 3,                 // read after Subordinate
    Subordinate = 4,          // read after Boss
    Boss = 5                  // highest priority, read first
};

// ----------------------------------------------------------------------------
// [GEMINI-MODIFIED] Email class. Gemini's original stored a category, subject,
// date, priority and numeric date. Validation, the enum, and an
// arrival sequence number (tie-breaker) were added.
// ----------------------------------------------------------------------------
class Email {
private:
    std::string categoryName;     // text of the sender category, e.g. "Boss"
    std::string subject;          // subject line (may contain spaces)
    std::string dateStr;          // original date text in MM-DD-YYYY format
    SenderCategory category;      // priority level derived from categoryName
    long dateVal;                 // date as YYYYMMDD so newer = larger number
    unsigned long sequence;       // arrival order, smaller = arrived earlier

    // [GEMINI-MODIFIED] Convert category text to enum; throws if unknown.
    static SenderCategory parseCategory(const std::string& cat) {
        if (cat == "Boss") return SenderCategory::Boss;                       // highest
        if (cat == "Subordinate") return SenderCategory::Subordinate;         // next
        if (cat == "Peer") return SenderCategory::Peer;                       // next
        if (cat == "ImportantPerson") return SenderCategory::ImportantPerson; // next
        if (cat == "OtherPerson") return SenderCategory::OtherPerson;         // lowest
        throw std::invalid_argument("unknown sender category");  // reject anything else
    }

    // [GEMINI-MODIFIED] Convert MM-DD-YYYY to YYYYMMDD; throws if malformed.
    // stoi/substr (which can throw or misparse) were replaced with a manual,
    // validated parse.
    static long parseDate(const std::string& d) {
        if (d.size() != 10 || d[2] != '-' || d[5] != '-')   // must look like MM-DD-YYYY
            throw std::invalid_argument("date must be MM-DD-YYYY");  // wrong shape
        for (std::size_t i = 0; i < d.size(); ++i) {        // check every character
            if (i == 2 || i == 5) continue;                 // skip the two dashes
            if (d[i] < '0' || d[i] > '9')                   // all others must be digits
                throw std::invalid_argument("date has non-digit characters");
        }
        int mm = (d[0] - '0') * 10 + (d[1] - '0');                                // month
        int dd = (d[3] - '0') * 10 + (d[4] - '0');                                // day
        int yyyy = (d[6] - '0') * 1000 + (d[7] - '0') * 100 +
                   (d[8] - '0') * 10 + (d[9] - '0');                              // year
        if (mm < 1 || mm > 12 || dd < 1 || dd > 31)         // basic range check
            throw std::invalid_argument("date out of range");
        return yyyy * 10000L + mm * 100L + dd;              // YYYYMMDD number
    }

public:
    // [GEMINI-MODIFIED] Constructor stores fields and derives priority values.
    Email(const std::string& cat, const std::string& subj,
          const std::string& date, unsigned long seq)
        : categoryName(cat), subject(subj), dateStr(date),   // copy the text fields
          category(parseCategory(cat)),                      // validate + convert category
          dateVal(parseDate(date)),                          // validate + convert date
          sequence(seq) {}                                   // remember arrival order

    // [GEMINI] Simple getters used when printing a NEXT result.
    const std::string& getCategory() const { return categoryName; }  // sender text
    const std::string& getSubject() const { return subject; }        // subject text
    const std::string& getDate() const { return dateStr; }           // date text

    // [GEMINI-MODIFIED] Returns true if THIS email has LOWER priority than other.
    // The MaxHeap puts the "greatest" (highest priority) email at the root.
    bool operator<(const Email& other) const {
        if (category != other.category)                      // different sender categories?
            return static_cast<int>(category) <
                   static_cast<int>(other.category);         // lower category = lower priority
        if (dateVal != other.dateVal)                        // same category, different date?
            return dateVal < other.dateVal;                  // older date = lower priority
        return sequence > other.sequence;                    // tie: later arrival = lower priority
    }
};

// ----------------------------------------------------------------------------
// [GEMINI-MODIFIED] MaxHeap class: list-based (std::vector) binary max-heap
// written from scratch. Gemini's sift logic is kept; a safe
// accessor and std::move use were added.
// ----------------------------------------------------------------------------
class MaxHeap {
private:
    std::vector<Email> heap;   // heap stored as a list: children of i are 2i+1, 2i+2

    // [GEMINI] Move the element at idx up until its parent is not smaller.
    void siftUp(std::size_t idx) {
        while (idx > 0) {                                    // stop at the root
            std::size_t parent = (idx - 1) / 2;              // index of the parent
            if (heap[parent] < heap[idx]) {                  // parent has lower priority?
                std::swap(heap[parent], heap[idx]);          // swap child above parent
                idx = parent;                                // continue from the parent spot
            } else {
                break;                                       // heap property restored
            }
        }
    }

    // [GEMINI] Move the element at idx down until it is not smaller than kids.
    void siftDown(std::size_t idx) {
        std::size_t size = heap.size();                      // current number of emails
        while (true) {                                       // loop until in place
            std::size_t left = 2 * idx + 1;                  // left child index
            std::size_t right = 2 * idx + 2;                 // right child index
            std::size_t largest = idx;                       // assume current is largest
            if (left < size && heap[largest] < heap[left])   // left child bigger?
                largest = left;                              // remember it
            if (right < size && heap[largest] < heap[right]) // right child bigger?
                largest = right;                             // remember it
            if (largest != idx) {                            // need to move down?
                std::swap(heap[idx], heap[largest]);         // swap with larger child
                idx = largest;                               // continue from that child
            } else {
                break;                                       // heap property restored
            }
        }
    }

public:
    // [GEMINI-MODIFIED] Add an email; O(log n). Takes by value and moves it in
    // to avoid an extra copy of the strings.
    void insert(Email email) {
        heap.push_back(std::move(email));                    // add at the end of the list
        siftUp(heap.size() - 1);                             // restore heap order
    }

    // [GEMINI] True when there are no emails.
    bool isEmpty() const { return heap.empty(); }

    // [GEMINI-MODIFIED] Number of unread emails (std::size_t, not int).
    std::size_t size() const { return heap.size(); }

    // [GEMINI-MODIFIED] Highest-priority email; O(1). Caller must check
    // isEmpty() first; throws instead of reading invalid memory.
    const Email& getMax() const {
        if (heap.empty())                                    // nothing to return?
            throw std::out_of_range("heap is empty");        // fail safely
        return heap[0];                                      // root is the maximum
    }

    // [GEMINI-MODIFIED] Remove the highest-priority email; O(log n). Safe no-op
    // when empty. Uses std::move to avoid copying strings.
    void removeMax() {
        if (heap.empty()) return;                            // READ on empty inbox: do nothing
        heap[0] = std::move(heap.back());                    // last element becomes the root
        heap.pop_back();                                     // shrink the list by one
        if (!heap.empty())                                   // anything left to fix?
            siftDown(0);                                     // restore heap order
    }
};

// ----------------------------------------------------------------------------
// [ADDED] CommandProcessor class: reads commands and drives the MaxHeap.
// Gemini put this logic inside main(); it is now an object so the program
// is object-oriented as the assignment requires.
// ----------------------------------------------------------------------------
class CommandProcessor {
private:
    std::istream& in;           // where commands are read from (the test file)
    std::ostream& out;          // where normal output goes (the terminal)
    MaxHeap inbox;              // the CEO's inbox as a priority queue
    unsigned long nextSeq;      // arrival counter given to each new email

    // [ADDED] Remove leading/trailing spaces, tabs and '\r' from a string.
    static std::string trim(const std::string& s) {
        const std::string ws = " \t\r\n";                    // characters to strip
        std::size_t first = s.find_first_not_of(ws);         // first real character
        if (first == std::string::npos) return "";           // all whitespace -> empty
        std::size_t last = s.find_last_not_of(ws);           // last real character
        return s.substr(first, last - first + 1);            // keep the middle part
    }

    // [GEMINI-MODIFIED] Handle "EMAIL <category>,<subject>,<date>".
    // Gemini's parsing is kept; trimming and error checks were added.
    void handleEmail(const std::string& data) {
        std::size_t firstComma = data.find(',');             // end of the category
        if (firstComma == std::string::npos)                 // no comma at all?
            throw std::invalid_argument("missing commas");
        std::size_t secondComma = data.find(',', firstComma + 1);  // end of the subject
        if (secondComma == std::string::npos)                // only one comma?
            throw std::invalid_argument("missing second comma");
        std::string cat = trim(data.substr(0, firstComma));  // sender category text
        std::string subj = trim(data.substr(firstComma + 1,
                                            secondComma - firstComma - 1));  // subject text
        std::string date = trim(data.substr(secondComma + 1));  // date text
        inbox.insert(Email(cat, subj, date, nextSeq));       // validate, build, and queue it
        ++nextSeq;                                           // only count emails that were accepted
    }

    // [GEMINI-MODIFIED] Handle NEXT: show the top email without removing it.
    void handleNext() {
        if (inbox.isEmpty()) return;                         // empty inbox: print nothing
        const Email& top = inbox.getMax();                   // highest-priority email
        out << "Next email:\n";                              // header line
        out << "Sender: " << top.getCategory() << "\n";      // sender category
        out << "Subject: " << top.getSubject() << "\n";      // subject line
        out << "Date: " << top.getDate() << "\n";            // date
    }

    // [GEMINI] Handle READ: CEO has read the email, so remove it.
    void handleRead() {
        inbox.removeMax();                                   // safe even if empty
    }

    // [GEMINI] Handle COUNT: print how many unread emails remain.
    void handleCount() {
        out << "There are " << inbox.size() << " emails to read.\n";  // required format
    }

public:
    // [ADDED] Constructor stores the input/output streams and starts counter.
    CommandProcessor(std::istream& input, std::ostream& output)
        : in(input), out(output), nextSeq(0) {}              // sequence starts at 0

    // [GEMINI-MODIFIED] Read every line of the file and run its command.
    void run() {
        std::string rawLine;                                 // one line of the file
        while (std::getline(in, rawLine)) {                  // read until end of file
            std::string line = trim(rawLine);                // remove '\r' and extra spaces
            if (line.empty()) continue;                      // skip blank lines
            try {                                            // catch bad input without crashing
                if (line.rfind("EMAIL ", 0) == 0)            // starts with "EMAIL "?
                    handleEmail(line.substr(6));             // pass everything after it
                else if (line == "NEXT")                     // NEXT command
                    handleNext();                            // show top email
                else if (line == "READ")                     // READ command
                    handleRead();                            // remove top email
                else if (line == "COUNT")                    // COUNT command
                    handleCount();                           // show unread count
                else                                         // anything else
                    std::cerr << "Skipping unknown command: " << line << "\n";
            } catch (const std::exception& e) {              // bad EMAIL line etc.
                std::cerr << "Skipping bad line (" << e.what() << "): " << line << "\n";
            }
        }
    }
};

// ----------------------------------------------------------------------------
// [GEMINI-MODIFIED] main only opens the file and hands control to the
// CommandProcessor object. Gemini's file-opening checks are kept.
// ----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {                                          // no file name given?
        std::cerr << "Usage: " << argv[0] << " <input_file>\n";  // show how to run
        return 1;                                            // exit with error code
    }
    std::ifstream file(argv[1]);                             // open the test file
    if (!file.is_open()) {                                   // could not open it?
        std::cerr << "Error opening file: " << argv[1] << "\n";  // tell the user
        return 1;                                            // exit with error code
    }
    CommandProcessor processor(file, std::cout);             // object that runs all commands
    processor.run();                                         // process the whole file
    return 0;                                                // success
}
