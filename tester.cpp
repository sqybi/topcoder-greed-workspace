/*
    Vexorian's c++ generic tester. Version 0.7
    Part of test template which has many features:
    - Test cases in bracket format for ease of modification.
    - Very small code for the problem file.
    - Test results use colors to make them easier to read and notice bad results.
    - When in *n*x operating systems, it runs each test case in a separate,
      forked process. A runtime error in a test case won't halt execution of
      later test cases. Globals don't need to  be initialized manually.

         This feature uses a /tmp/topcoder_vx_tester_communicate file to share
         info between processes. You can change the path if needed by modifying
         the constant below.

    - Can temporarily disable test cases.
    - Three verbosity modes:
       * FULL_REPORT : First runs all test cases with verbose output. Then shows
                       a brief report with most relevant info.
       * NO_REPORT   : If you don't need the report, it is replaced by a single
                       line of colored result characters and score.
       * ONLY_REPORT : Shows only the mentioned report.

    Everything is about the huge runTests function, read its documentation.

    (c) 2013 Victor Hugo Soliz Kuncar, ZLIB/LibPNG license
*
*/

using namespace std;

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    #define DISABLE_THREADS
#endif
#ifndef DISABLE_THREADS
    #define DO_THREADS
#endif
#ifdef DO_THREADS
   #include <unistd.h>
   #include <sys/wait.h>
   #include <sys/types.h>
#endif

namespace Tester {

const int    FULL_REPORT    = 0;
const int    COMPACT_REPORT = 1;
const int    ONLY_REPORT    = 2;

// Name the color codes:
#ifdef DISABLE_COLORS
    const string COLOR_RESET  = "";

    const string BRIGHT_GREEN   = "";
    const string BRIGHT_RED     = "";
    const string NORMAL_CROSSED = "";
    const string RED_BACKGROUND = "";
    const string NORMAL_FAINT   = "";
    const string BRIGHT_CYAN    = "";
#else
    const string COLOR_RESET  = "\033[0m";

    const string BRIGHT_GREEN   = "\033[1;32m";
    const string BRIGHT_RED     = "\033[1;31m";
    const string NORMAL_CROSSED = "\033[0;9;37m";
    const string RED_BACKGROUND = "\033[1;41m";
    const string NORMAL_FAINT   = "\033[0;2m";
    const string BRIGHT_CYAN    = "\033[1;36m";
#endif

// Configuration:
const string COMMUNICATION_FILE = "/tmp/topcoder_vx_tester_communicate";
const int    MAX_REPORT_LENGTH = 73;
const int    BAR_LENGTH        = 73; //length of the bar === that separates test cases
const string BAR_COLOR    = NORMAL_FAINT;  //The bar ==== color
const string TEST_COLOR   = BRIGHT_CYAN; //Color for the word "Test" or "T"

const string GRADE_COLOR[] = {
    RED_BACKGROUND, // bad (overall),
    BRIGHT_RED,   // bad (case),
    COLOR_RESET,  // neutral
    BRIGHT_GREEN  // good
};

const map<char, string> RESULT_COLOR = {
    {'+' , BRIGHT_GREEN},
    {'X' , BRIGHT_RED},
    {'E' , RED_BACKGROUND },
    {'T' , BRIGHT_RED},
    {'?' , COLOR_RESET},
    {'d' , NORMAL_CROSSED},
};

/*==============================================================================
   output struct, holds the expected result of execution or is marked as an
   unknown result */
template<typename T> struct output {
    T value; bool u=false;

    output(T value) { this->value = value; }
    output() { u = true; }
};

/*==============================================================================
  Converts given argument into a string containing pretty-looking output
*/
string pretty(string s)
{
    return "\"" + s + "\"";
}
string pretty(double x)
{
    ostringstream s;
    s.precision(10);
    s << x;
    return s.str();
}
template <typename T> string pretty(T t) {
    ostringstream s;
    s << t;
    return s.str();
}
template <typename T> string pretty(vector<T> t)
{
    ostringstream s;
    s << "{";
    for (int i=0; i<t.size(); i++) {
        s << (i?",":"") << pretty(t[i]);
    }
    s << "}";
    return s.str();
}

/*==============================================================================
  Prints a test case's argument list

  printArgs(a,b,c) ->  cout << "[" << pretty(a) <<","<< pretty(b) <<"," <<  pretty(c) <<"]"
*/
void printArgsCont() {}

template<typename T, typename... Args> void printArgsCont(T value, Args... args)
{
    cout << "," << pretty(value);
    printArgsCont(args...);
}

template<typename T, typename... Args> void printArgs(T value, Args... args)
{
    cout << "[" << pretty(value);
    printArgsCont(args...);
    cout << "]";
}

/*==============================================================================
 tc_eq compares two values using TopCoder's rules. Mostly to allow floating
 point comparison using absolute/relative error

*/
bool tc_eq(const double &a, const double &b)
{
    return (a==a) && (b==b) && fabs(a - b) <= 1e-9 * max(1.0, fabs(a));
}
template<typename T> bool tc_eq(const T &a, const T &b)
{
    return a == b;
}
template<typename T> bool tc_eq(const vector<T> &a, const vector<T> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (int i = 0; i < a.size(); i++) {
        if (! tc_eq(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

/*==============================================================================
   Add color to a string according to grade q. -2 is worst, 1 is good.
*/
string colorString(string s, int q)
{
    return GRADE_COLOR[q+2] + s + COLOR_RESET;
}
string colorTestResult(char r)
{
    return RESULT_COLOR.at(r) + r + COLOR_RESET;
};

/*==============================================================================
   Print a double value using two decimals
*/
string f02(double x)
{
    ostringstream s;
    s.precision(2);
    s << fixed <<x;
    return s.str();
}

/*==============================================================================
   Prints case number i if there are n test cases in total. So it uses the
   correct width.
*/
string caseNum(int i, int n) {
    ostringstream ss;
    int w =  max<int>(1, to_string(n-1).length() );
    ss << setw(w) << i;
    return ss.str();
}

/*==============================================================================
    Returns the name of a type in human-readable form. It likely only works 100%
    correctly with classes that don't belong to a namespace and is not very
    portable.
*/
template<typename T> string getTypeName()
{
    string s = typeid(T).name();
    int i= 0;
    while ( (i < s.size()) && ('0' <= s[i] && s[i] <= '9') ) {
        i++;
    }
    return s.substr(i);
}

/*==============================================================================*/
int noColorLength(const string &x)
{
    int len = 0;
    bool colorCode = false;
    for (char ch : x) {
        if (ch == '\033') {
            colorCode = true;
        } else if (colorCode && ch == 'm') {
            colorCode = false;
        } else {
            len += ! colorCode;
        }

    }
    return len;
}

/*=============================================================================*/
#ifdef DO_THREADS
    bool decodeStatus(int status, string & message, string & report)
    {
        if (WIFSIGNALED(status)) {
            switch ( WTERMSIG(status) ) {
            case SIGSEGV:
                message = "Segmentation fault.";
                report = "(segfault)";
                return true;
            case SIGABRT:
                message = "Program aborted.";
                report = "(aborted)";
                return true;
            case SIGFPE:
                message = "Arithmetic error (e.g. division by zero)";
                report = "(arithmetic)";
                return true;
            case SIGINT:
                message = report = "SIGINT";
                return true;
            case SIGTERM:
                message = "Killed by another process";
                report = "(killed)";
                return true;
            }
        }
        return false;
    }

    string readCommunication()
    {
        string comm = "E";
        ifstream f(COMMUNICATION_FILE);
        if (f) {
            getline(f, comm);
            if ( comm.size() == 0 ) {
                comm = "E";
            }
            f.close();
        }
        return comm;
    }

    void writeCommunication(string comm)
    {
        ofstream f(COMMUNICATION_FILE);
        f << comm;
        f.close();
    }
#endif

/** =============================================================================
    runTestCase tests a single case.

    Template arguments:
    @arg input       : The class that stores input.
    @arg output      : The class that stores output.
    @arg problemClass: The class name for the problem.

    Normal arguments:
    @arg caseNo    : The test case's index
    @arg in        : The test case's input
    @arg exp       : The test case's expected output
    @arg n         : The number of testcases
    @arg timeOut   : The maximum time available for the test case.

    Output arguments:
    @arg finalLines: Vector that countains the results card that is shown at the
                     end of tests.

    The return is one of:
    + : Correct.
    X : Wrong answer.
    T : Correct but time exceeded T
    ? : Executed correctly, there is no expected output so we can't know if it
        is correct

*/
template<typename input, typename output, typename problemClass>
char runTestCase(int caseNo, input in, output exp, int n, double timeOut, string & reportLine, bool silentMode) {
    #ifdef DO_THREADS
        pid_t thid = fork();
        bool testThread = ( thid == 0 );
    #else
        bool testThread = true;
    #endif
    char ret = '?';
    string report;
    if (testThread) {
        if (! silentMode) {
          cout << "shik" << endl;
          cout << COLOR_RESET << endl;
            cout << TEST_COLOR << "Test " << caseNo << COLOR_RESET <<": ";
            in.print();
            cout << endl;
        }
        time_t s = clock();
        problemClass* instance = new problemClass();
        output res = output( in.run(instance) );
        double elapsed = (double)(clock() - s) / CLOCKS_PER_SEC;
        delete instance;

        if (! silentMode) {
            cout << "Time: "<< f02(elapsed)<<" seconds." << endl;
        }

        bool correct = exp.u || tc_eq(exp.value, res.value);

        if (! silentMode) {
            if (! correct) {
                cout << "Desired answer:" << endl;
                cout << "\t" << pretty(exp.value) << endl;
            }
            cout << "Your answer:" << endl;
            cout << "\t" << pretty(res.value) << endl;
        }

        ret = '-';
        if (! correct) {
            ret = 'X';
        } else if (elapsed > timeOut ) {
            ret = 'T';
        } else if (exp.u) {
            ret = '?';
        } else {
            ret = '+';
        }
        report = " (" + f02(elapsed) + "s)";
        if (ret == 'X' || ret == '?') {
            report += " [" + pretty(res.value)+"]";
        }
        #ifdef DO_THREADS
            writeCommunication(ret + report);
            exit(0);
        #endif
    } else {
        #ifdef DO_THREADS
            int child_status;
            while ( wait(&child_status) != thid ) {
            }
            if ( child_status != 0){
                ret = 'E';
                string message;
                if (! decodeStatus(child_status, message, report)) {
                    report = "Exit code: " + to_string(child_status);
                    message = report;
                }
                report = " " + report;
                if (! silentMode) {
                    cout << message << endl;
                }
                ret = 'E';
            } else {
                ret = readCommunication()[0];
                if (ret != 'E') {
                    report = readCommunication().substr(1);
                }
            }
        #endif
    }
    string tm = " " + colorTestResult(ret);
    if (! silentMode) {
        cout << tm;
        cout << endl << BAR_COLOR << string(BAR_LENGTH,'=') << COLOR_RESET << endl;
    }
    string ln = " " + TEST_COLOR + "t" + caseNum(caseNo,n) +":" + tm + report;
    int s = noColorLength(ln);
    if (s > MAX_REPORT_LENGTH) {
        int dif = ln.length() - s;
        ln = ln.substr(0, dif + MAX_REPORT_LENGTH - 3) + "...";
    }
    reportLine = ln;
    return ret;
}

/** =============================================================================
    runTests tests a sequence of test cases, outputs results.

    Template arguments:
    @arg problemClass: The class name for the problem.
    @arg input       : The class that stores input.
    @arg output      : The class that stores output.

    Normal arguments:
    @arg testCases   : A vector of test cases, a test case is a (input,output) pair.
    @arg disabled    : A function takes integer returns boolean. If disabled(i) then
                       the i-th test case is disabled.
    @arg score       : The score of the problem (e.g: 250)
    @arg openTime    : The time in which the problem was opened, used to calculate
                       submission score.
    @arg caseTimeOut : The maximum time available for each  test case.
    @arg compact     : Use compact output.

    The return is a program exit code.

*/
template<typename problemClass, typename input, typename output>
int runTests(vector<pair<input,output>> testCases, function<bool(int)> disabled, int score, int openTime, double caseTimeOut, int compactLevel)
{
    string header = getTypeName<problemClass>() + ( (compactLevel != 1)  ? "\n\n" : ": ");
    if (compactLevel == 2) {
        cout << header; cout.flush();
    }

    string result;
    vector<string> finalLines;
    int n = testCases.size();
    for (int i = 0; i < n; i++) {
        char ch = 'd';
        string reportLine;
        if (disabled(i)) {
            reportLine = " " + TEST_COLOR + "t" + caseNum(i, n)+ COLOR_RESET + ": "+colorTestResult('d') ;
        } else {
            ch = runTestCase<input,output,problemClass>(
                i, testCases[i].first, testCases[i].second,
                n, caseTimeOut, reportLine, (compactLevel == 2)
            );
        }
        result += ch;
        if (compactLevel == 2) {
            cout << reportLine << endl;
        } else {
            finalLines.push_back(reportLine);
        }
    }
    if ( compactLevel != 2) {
        cout << header ;
    }
    bool good = true;
    for (char ch: result) {
        good &= ( ch == '?' || ch == '+' );
        if (compactLevel == 1) {
            cout << colorTestResult(ch) << " ";
        }
    }
    if (compactLevel == 0) {
        for (string ln : finalLines) {
            cout << ln << endl;
        }
    }

    int T = time(NULL) - openTime;
    double PT = T / 60.0, TT = 75.0, SS = score * (0.3 + (0.7 * TT * TT) / (10.0 * PT * PT + TT * TT));
    string SSS = Tester::f02(SS);
    SSS = colorString(SSS, good? 1: -2 );
    if ( compactLevel != 1 ) {
        cout << "\n" << SSS << endl;
    } else {
        cout << "(" << SSS << ")." << endl;
    }
    return 0;
}

}
