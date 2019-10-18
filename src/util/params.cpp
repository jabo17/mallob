
#include "assert.h"

#include "params.h"
#include "console.h"

/**
 * Taken from Hordesat:ParameterProcessor.h by Tomas Balyo.
 */
void Parameters::init(int argc, char** argv) {
    setDefaults();
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        if (arg[0] != '-') {
            filename = std::string(arg);
            continue;
        }
        char* eq = strchr(arg, '=');
        if (eq == NULL) {
            params[arg+1];
        } else {
            *eq = 0;
            char* left = arg+1;
            char* right = eq+1;
            params[left] = right;
        }
    }
}

void Parameters::setDefaults() {
    setParam("p", "5.0"); // rebalance period (seconds)
    setParam("l", "0.95"); // load factor
    setParam("c", "1"); // num clients
    setParam("t", "2"); // num threads per node
    setParam("v", "2"); // verbosity 0=CRIT 1=WARN 2=INFO 3=VERB 4=VVERB ...
    setParam("vhorde", "0"); // hordesat verbosity
    setParam("td", "0.01"); // temperature decay for thermodyn. balancing
    setParam("log", "."); // logging directory
    //setParam("colors"); // colored terminal output
    //setParam("h"); setParam("help"); // print usage
}

void Parameters::printUsage() {

    Console::log(Console::INFO, "Usage: mallob [options] <scenario>");
    Console::log(Console::INFO, "<scenario> : File path and name prefix for client scenario(s);");
    Console::log(Console::INFO, "             will parse <name>.0 for one client, ");
    Console::log(Console::INFO, "             <name>.0 and <name>.1 for two clients, ...");
    Console::log(Console::INFO, "Options:");
    Console::log(Console::INFO, "-c=<num-clients>      Amount of client nodes (int c >= 1)");
    Console::log(Console::INFO, "-colors               Colored terminal output based on messages' verbosity");
    Console::log(Console::INFO, "-h|-help              Print usage");
    Console::log(Console::INFO, "-l=<load-factor>      Load factor to be aimed at (0 < l < 1)");
    Console::log(Console::INFO, "-log=<log-dir>        Directory to save logs in (default: .)");
    Console::log(Console::INFO, "-p=<rebalance-period> Do global rebalancing every r seconds (r > 0)");
    Console::log(Console::INFO, "-t=<num-threads>      Amount of worker threads per node (int t >= 1)");
    Console::log(Console::INFO, "-v=<verb-num>         Logging verbosity: 0=CRIT 1=WARN 2=INFO 3=VERB 4=VVERB ...");
    Console::log(Console::INFO, "-vhorde=<verb-num>    Logging verbosity of hordesat components");
}

string Parameters::getFilename() {
  return filename;
}

void Parameters::printParams() {
    std::string out = "";
    for (map<string,string>::iterator it = params.begin(); it != params.end(); it++) {
        if (it->second.empty()) {
            out += it->first + ", ";
        } else {
            out += it->first + "=" + it->second + ", ";
        }
    }
    out = out.substr(0, out.size()-2);
    Console::log(Console::INFO, "Called with parameters: %s", out.c_str());
}

void Parameters::setParam(const char* name) {
    params[name];
}

void Parameters::setParam(const char* name, const char* value) {
    params[name] = value;
}

bool Parameters::isSet(const string& name) {
    return params.find(name) != params.end();
}

string Parameters::getParam(const string& name, const string& defaultValue) {
    if (isSet(name)) {
        return params[name];
    } else {
        return defaultValue;
    }
}

string Parameters::getParam(const string& name) {
    return getParam(name, "ndef");
}

int Parameters::getIntParam(const string& name, int defaultValue) {
    if (isSet(name)) {
        return atoi(params[name].c_str());
    } else {
        return defaultValue;
    }
}

int Parameters::getIntParam(const string& name) {
    assert(isSet(name));
    return atoi(params[name].c_str());
}

float Parameters::getFloatParam(const string& name, float defaultValue) {
    if (isSet(name)) {
        return atof(params[name].c_str());
    } else {
        return defaultValue;
    }
}

float Parameters::getFloatParam(const string& name) {
    assert(isSet(name));
    return atof(params[name].c_str());
}
