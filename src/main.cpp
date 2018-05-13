#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm> 
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <pthread.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <exception>
#include <iomanip>
#include <poll.h>

#include "cgicc/CgiDefs.h"
#include "cgicc/Cgicc.h"
#include "cgicc/HTTPHTMLHeader.h"
#include "cgicc/HTMLClasses.h"

#define BT_DEVICE_NODE      "/dev/rfcomm0"
#define BT_DEVICE_ADDRESS   "00:12:6F:27:ED:9A"
#define BT_CONN_CHANNEL     "1"

static int debug_1 = 0;
void debug_msg(const std::string& text);


static std::string debug=("");


struct cgi_data {
public:
    cgi_data() : isBtConnected(false) {}

//private:
    bool isBtConnected;
    std::string btConnState;
    std::string btConnColor;
    std::string btDisconnColor;
    std::string btConnAccess;
    std::string btDisconnAccess;
    std::string btSignalState;

    std::string btSpeed;
    std::string btSignal;
    std::string btLight;

    void process_data(void);
};

class cgi_input {
public:
    cgi_input(): name(""), value(""), default_value("") {}
    cgi_input(const std::string& nam, const std::string& val, const std::string& def): name(nam), value(val), default_value(def) {}

    const std::string& get_name(void) const { return name; }
    const std::string& get_value(void) const { return value; }

    void set_value(const std::string& val) { value = val; }
    void set_default(void) { value = default_value; }
private:
    std::string name;
    std::string value;
    std::string default_value;
};

class bt_handler {
public:
    bt_handler() {}

    bool is_connection_active(void);
    void get_data(cgi_data& data);
    void manage_connection(const std::string& cmd);
    void manage_speed(const std::string& cmd);
    void manage_signal(const std::string& cmd);
    void manage_light(const std::string& cmd);
private:

    void process_status(std::string& status, cgi_data& data);
    void read_device(std::string& buff);
    void write_device(const std::string& buff);   
};

class cgi_handler {
public:
    enum input_kind { first = 0, bluetooth = first, light, speed, signal, last };
    cgi_handler();

    void extract_inputs(void);
    void manage_bt(void);
    void process_cgi_data(void);
    void generate_html(void);
    const std::string& get_input(const input_kind& kind) const { return inputs.at(kind).get_value(); }

private:
    std::vector<cgi_input> inputs;

    cgi_data data;
    bt_handler btHandler;
};

void bt_handler::process_status(std::string& status, cgi_data& data)
{
    if(status.empty() != true) {
        std::size_t pos_start = 0;
        std::size_t pos_end = 0;

        pos_start = status.find("Speed=");
        pos_start = status.find("=", pos_start)+1;
        pos_end = status.find(';', pos_start);
        data.btSpeed = status.substr(pos_start, pos_end-pos_start);

        pos_start = status.find("Horn=");
        pos_start = status.find("=", pos_start)+1;
        pos_end = status.find(';', pos_start);
        data.btSignal = status.substr(pos_start, pos_end-pos_start);

        pos_start = status.find("Light=");
        pos_start = status.find("=", pos_start)+1;
        pos_end = status.find(';', pos_start);    
        data.btLight = status.substr(pos_start, pos_end-pos_start);

        data.isBtConnected = true;
    }
}

void cgi_data::process_data(void)
{
    if(isBtConnected == true) {
        btConnState = "Connected";
        btConnColor = "green";
        btDisconnColor = "";
        btConnAccess = "disabled";
        btDisconnAccess = "enabled";

        if(btSignal == "0") {
            btSignalState = "activate";
        }
        else if(btSignal == "1") {
            btSignalState = "deactivate";
        }
        else {
            btSignalState = "";
        }
    }
    else {
        btConnState = "Disconnected";
        btConnColor = "";
        btDisconnColor = "red";
        btConnAccess = "enabled";
        btDisconnAccess = "disabled";
    }
}

cgi_handler::input_kind& operator++(cgi_handler::input_kind& kind)
{
    kind = cgi_handler::input_kind(kind+1);

    return kind;
}

cgi_handler::cgi_handler(void): inputs(last)
{
    inputs.at(0) = cgi_input("bluetooth", "", "");
    inputs.at(1) = cgi_input("leds", "", "");
    inputs.at(2) = cgi_input("speed", "", "");
    inputs.at(3) = cgi_input("signal", "", "");
}

void cgi_handler::extract_inputs(void)
{
    cgicc::Cgicc form;

    for(input_kind i = first; i < last; ++i) {
        cgicc::form_iterator it = form.getElement(inputs.at(i).get_name());
        if(it == form.getElements().end() || it->getValue() == "") {
            inputs[i].set_default();
        }
        else {
            inputs[i].set_value(it->getValue());
        }
    }
}

void cgi_handler::manage_bt(void)
{
    std::string input("");

    input = get_input(cgi_handler::bluetooth);
    btHandler.manage_connection(input);

    if(btHandler.is_connection_active() == true) {
        input = get_input(cgi_handler::light);
        btHandler.manage_light(input);

        input = get_input(cgi_handler::speed);
        btHandler.manage_speed(input);

        input = get_input(cgi_handler::signal);
        btHandler.manage_signal(input);

        btHandler.get_data(data);
        data.process_data();
    }
}

void cgi_handler::process_cgi_data(void)
{
    data.process_data();
}

void cgi_handler::generate_html(void)
{
    const char* server_addr = getenv("SERVER_NAME");
    const char* client_addr = getenv("REMOTE_ADDR");
    FILE *file = NULL;
    char os_name[100+1] = {0};

    file = popen("uname -o", "r");
    if(file != NULL) {
        fgets(os_name, sizeof(os_name), file);
    }

    char dateBuff[100] = {0};
    char tempBuff[12] = {0};
    file = popen("date \"+%A %d/%m/%y %H:%M\"", "r");
    fgets(dateBuff, sizeof(dateBuff), file);

    FILE* fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    fgets(tempBuff, sizeof(tempBuff), fp);
    fclose(fp);

    double tempVal = atoi(tempBuff);
    tempVal /= 1000;

    std::cout<<cgicc::HTTPHTMLHeader()<<std::endl;
    std::cout<<cgicc::html()<<cgicc::head()<<cgicc::title("AMUv2 Web Interface")<<cgicc::head()<<std::endl;
    std::cout<<cgicc::body()<<cgicc::h2("Remote Web Interface for AMUv2 test release")<<std::endl;
    std::cout<<"<form action=\"/cgi/amu.cgi\" id=\"app_client\" method=\"POST\">"<<std::endl;

    std::cout<<"Server OS: "<<os_name<<" "<<std::endl;
    if(server_addr != NULL) {
        std::cout<<"[Server address: "<<server_addr<<"] ";
    }
    if(client_addr != NULL) {
        std::cout<<"[Client address: "<<client_addr<<"]</br>"<<std::endl;
    }

    std::cout<<"</br>"<<std::endl;
    std::cout<<"Date/Time: "<<dateBuff<<std::endl;
    std::cout<<"<br>"<<std::endl;
    std::cout<<"CPU temperature: "<<std::setprecision(1)<<std::fixed<<tempVal<<" &#176C"<<std::endl;
    std::cout<<"</br>"<<std::endl;
    std::cout<<"<hr align=\"left\" width=\"60%\">"<<std::endl;
    std::cout<<"<br>"<<std::endl;

    std::cout<<"<table width=\"780\" height=\"500\" border=0>"<<std::endl;
    std::cout<<"<tr>"<<std::endl;
        std::cout<<"<td align=\"left\" valign=\"middle\">"<<std::endl;
        std::cout<<"<table border=\"1\" style=\"border-collapse:collapse\" width=\"200\" height=\"150\">"<<std::endl;
        std::cout<<"<tr>"<<std::endl;
            std::cout<<"<td style=\"text-align:center\" height=\"20%\" colspan=\"2\">Bluetooth connection</td>"<<std::endl;
        std::cout<<"</tr><tr>"<<std::endl;

        std::cout<<"<td style=\"background-color:"<<data.btConnColor<<"; text-align:center\"><INPUT type=\"submit\" name=\"bluetooth\" value=\"connect\" style=\"height:50px; width:80px\" "<<data.btConnAccess<<" </td>"<<std::endl;
            std::cout<<"<td style=\"background-color:"<<data.btDisconnColor<<"; text-align:center\"><INPUT type=\"submit\" name=\"bluetooth\" value=\"disconnect\" style=\"height:50px; width:80px\" "<<data.btDisconnAccess<<" ></td>"<<std::endl;
            std::cout<<"</tr>"<<std::endl;
        std::cout<<"</table>"<<std::endl;

        std::cout<<"</td><td align=\"right\" valign=\"top\" colspan=\"2\">"<<std::endl;
        std::cout<<"<table border=\"1\" style=\"border-collapse:collapse\" width=\"465\" height=\"250\">"<<std::endl;
        std::cout<<"<tr>"<<std::endl;
            std::cout<<"<td style=\"text-align:center\" height=\"14%\">AMU status data</td>"<<std::endl;
        std::cout<<"</tr><tr>"<<std::endl;
            std::cout<<"<td align=\"center\" valign=\"middle\">"<<std::endl;
            std::cout<<"<table border=\"0\">"<<std::endl;
            std::cout<<"<tr>"<<std::endl;
                std::cout<<"<td><input value=\"Real Time View: \" size=\"14\" style=\"border:none;\" readonly>"<<std::endl;
                std::cout<<"<input name=\"real_time\" value=\"DISABLED\" style=\"border:none;color:red\" readonly></td>"<<std::endl;
            std::cout<<"</tr>"<<std::endl;
            std::cout<<"<tr><td><hr></td></tr>"<<std::endl;
            std::cout<<"<tr>"<<std::endl;
                std::cout<<"<td><input value=\"Bluetooth:\" size=\"7\" style=\"border:none;\" readonly>"<<std::endl;
                std::cout<<"<input name=\"bt_connected\" value=\""<<data.btConnState<<"\" style=\"border:none;\" readonly></td>"<<std::endl;
            std::cout<<"</tr><tr>"<<std::endl;
                std::cout<<"<td><input value=\"Lighting:\" size=\"7\" style=\"border:none;\" readonly>"<<std::endl;
                std::cout<<"<input name=\"light_state\" value=\""<<data.btLight<<"\" style=\"border:none;\" readonly></td>"<<std::endl;
            std::cout<<"</tr><tr>"<<std::endl;
                std::cout<<"<td><input value=\"Speed:\" size=\"7\" style=\"border:none;\" readonly>"<<std::endl;
                std::cout<<"<input name=\"speed_state\" value=\""<<data.btSpeed<<"\" style=\"border:none;\" readonly></td>"<<std::endl;
            std::cout<<"</tr><tr>"<<std::endl;
                std::cout<<"<td><input value=\"Horn:\" size=\"7\" style=\"border:none;\" readonly>"<<std::endl;
                std::cout<<"<input name=\"horn_state\" value=\""<<data.btSignal<<"\" style=\"border:none;\" readonly></td>"<<std::endl;
            std::cout<<"</tr>"<<std::endl;
            std::cout<<"<tr><td><hr></td></tr>"<<std::endl;
            std::cout<<"<tr>"<<std::endl;
                std::cout<<"<td><input value=\"System info:\" size=\"10\" style=\"border:none;\" readonly>"<<std::endl;
            std::cout<<"</tr>"<<std::endl;
            std::cout<<"</table>"<<std::endl;
            std::cout<<"</td>"<<std::endl;
        std::cout<<"</tr>"<<std::endl;
        std::cout<<"</table>"<<std::endl;
        std::cout<<"</td>"<<std::endl;
    std::cout<<"</tr><tr>"<<std::endl;
        std::cout<<"<td align=\"left\" valign=\"top\">"<<std::endl;
        std::cout<<"<table border=\"1\" style=\"border-collapse:collapse\" width=\"260\" height=\"200\">"<<std::endl;
        std::cout<<"<tr>"<<std::endl;
            std::cout<<"<td style=\"text-align:center\" height=\"20%\" colspan=\"3\">Front leds control</td>"<<std::endl;
        std::cout<<"</tr><tr>"<<std::endl;
            std::cout<<"<td style=\"text-align:center\"><INPUT type=\"submit\" name=\"leds\" value=\"left\"  style=\"height:40px; width:50px\" enabled></td>"<<std::endl;
            std::cout<<"<td style=\"text-align:center\"><INPUT type=\"submit\" name=\"leds\" value=\"inner\" style=\"height:40px; width:50px\" enabled></td>"<<std::endl;
            std::cout<<"<td style=\"text-align:center\"><INPUT type=\"submit\" name=\"leds\" value=\"right\" style=\"height:40px; width:50px\" enabled></td>"<<std::endl;
        std::cout<<"</tr><tr>"<<std::endl;
            std::cout<<"<td style=\"background-color:lightgrey\"></td>"<<std::endl;
            std::cout<<"<td style=\"text-align:center\"><INPUT type=\"submit\" name=\"leds\" value=\"outer\" style=\"height:40px; width:50px\" enabled></td>"<<std::endl;
            std::cout<<"<td style=\"background-color:lightgrey\"></td>"<<std::endl;
        std::cout<<"</tr>"<<std::endl;
        std::cout<<"</table>"<<std::endl;
        std::cout<<"'</td><td align=\"center\" valign=\"top\">"<<std::endl;
        std::cout<<"<table border=\"1\" style=\"border-collapse:collapse\" width=\"250\" height=\"200\">"<<std::endl;
        std::cout<<"<tr>"<<std::endl;
            std::cout<<"<td style=\"text-align:center\" height=\"20%\" colspan=\"2\">Speed modification</td>"<<std::endl;
        std::cout<<"</tr><tr>"<<std::endl;
        std::cout<<"<td style=\"text-align:center\"><INPUT type=\"submit\" name=\"speed\" value=\"up\" style=\"height:50px; width:80px\" enabled></td>"<<std::endl;
        std::cout<<"<td style=\"text-align:center\"><INPUT type=\"submit\" name=\"speed\" value=\"down\" style=\"height:50px; width:80px\" enabled></td>"<<std::endl;
        std::cout<<"</tr>"<<std::endl;
        std::cout<<"</table>"<<std::endl;
        std::cout<<"</td><td align=\"right\" valign=\"top\">"<<std::endl;
        std::cout<<"<table border=\"1\" style=\"border-collapse:collapse\" width=\"150\" height=\"200\">"<<std::endl;
        std::cout<<"<tr>"<<std::endl;
            std::cout<<"<td style=\"text-align:center\" height=\"20%\">Horn signal</td>"<<std::endl;
        std::cout<<"</tr><tr>"<<std::endl;
            std::cout<<"<td style=\"text-align:center\"><INPUT type=\"submit\" name=\"signal\" value=\""<<data.btSignalState<<"\" style=\"height:50px; width:100px\" enabled></td>"<<std::endl;
        std::cout<<"</tr>"<<std::endl;
        std::cout<<"</table>"<<std::endl;
        std::cout<<"</td>"<<std::endl;
    std::cout<<"</tr>"<<std::endl;
    std::cout<<"</table>"<<std::endl;

    std::cout<<"</form>"<<std::endl;
    std::cout<<cgicc::body()<<cgicc::html()<<std::endl;
}

void debug_msg(const std::string& text)
{
    FILE* fp = NULL;

    fp = fopen("/tmp/debug.log", "a");
    if(fp != NULL) {
        fputs(text.c_str(), fp);
        fclose(fp);
    }   
}

void debug_out(const std::string& text)
{
    if(debug_1 != 0) {
        std::cout<<"Dbg text: "<<text<<std::endl;   
    }
}

bool bt_handler::is_connection_active(void)
{
    bool ret = false;

    int fd = open(BT_DEVICE_NODE, O_RDONLY);
    if(fd >= 0) {
        close(fd);

        usleep(10000);
        if(access(BT_DEVICE_NODE, F_OK) == 0) {
            ret = true;
        }
    }

    return ret;
}

void bt_handler::read_device(std::string& buff)
{
    int timoutMs = 2000;
    int fd = (-1);
 
    buff.clear();

    fd = open(BT_DEVICE_NODE, O_RDONLY);
    if(fd >= 0) {
        struct pollfd pollInfo = { .fd = fd, .events = POLLIN, .revents = 0 };

        int ret = poll(&pollInfo, 1, timoutMs);
        if(ret > 0 && (pollInfo.revents & POLLIN) != 0) {
            char byte = '\0';

            while(read(fd, &byte, 1) > 0) {
                if(byte == '\n') {
                    break;
                }
                else {
                    buff.push_back(byte);
                }
            }
        }

        close(fd);
    }
}

void bt_handler::write_device(const std::string& buff)
{
    FILE* fp = NULL;

    fp = fopen(BT_DEVICE_NODE, "a");
    if(fp != NULL) {
        fputs(buff.c_str(), fp);
        fclose(fp);
    }
}

void bt_handler::get_data(cgi_data& data)
{
    const std::string command(">all<");
    std::string buff("");
    
    write_device(command);
    read_device(buff);

    process_status(buff, data);
}

void bt_handler::manage_connection(const std::string &cmd)
{
    int timeoutSecs = 5;

    if(is_connection_active() == false) {
        if(cmd == "connect") {
            std::string system_command("");
            
            system_command.append("/usr/bin/rfcomm -r connect");
            system_command.push_back(' ');
            system_command.append(BT_DEVICE_NODE);
            system_command.push_back(' ');
            system_command.append(BT_DEVICE_ADDRESS);
            system_command.push_back(' ');
            system_command.append(BT_CONN_CHANNEL);
            system_command.push_back(' ');
            system_command.append("> /dev/null 2>&1 &");
            
            system(system_command.c_str());

            while(timeoutSecs > 0) {
                if(is_connection_active() == true) {
                    usleep(10000);
                    break;
                }
                else {
                    timeoutSecs--;
                    sleep(1);
                }
            }
        }
    }
    else {
        if(cmd == "disconnect") {
            system("killall rfcomm > /dev/null 2>&1");

            while(timeoutSecs > 0) {
                if(is_connection_active() == false) {
                    usleep(10000);
                    break;
                }
                else {
                    timeoutSecs--;
                    sleep(1);
                }
            }
        }
    }
}


void bt_handler::manage_speed(const std::string& cmd)
{
    char trigger = '\0';

    do {
        if(cmd == "up") {
            trigger = 'q';
        }
        else if(cmd == "down") {
            trigger = 'e';
        }
        else {
            break;
        }

        const std::string system_command(1, trigger);
        write_device(system_command);
        
        std::string response("");
        read_device(response);
        debug=response;
    } while(0);
}

void bt_handler::manage_signal(const std::string& cmd)
{
    char trigger = '\0';

    do {
        if(cmd == "activate") {
            trigger = 'm';
        }
        else if(cmd == "deactivate") {
            trigger = 'n';
        }
        else {
            break;
        }

        const std::string system_command(1, trigger);
        write_device(system_command);

        std::string response("");
        read_device(response);
        debug=response;
    } while(0);
}


void bt_handler::manage_light(const std::string& cmd)
{
    char trigger = '\0';

    do {
        if(cmd == "left") {
            trigger = 'i';
        }
        else if(cmd == "right") {
            trigger = 'p';
        }
        else if(cmd == "inner") {
            trigger = 'o';
        }
        else if(cmd == "outer") {
            trigger = 'l';
        }
        else {
            break;
        }

        const std::string system_command(1, trigger);
        write_device(system_command);

        std::string response("");
        read_device(response);
        debug=response;
    } while(0);
}


int main(int argc, char** argv)
{
    static int test123 = 0;

    if(test123 == 0) {
        test123 = 5;
    }

    cgi_handler cgi;
    cgi.extract_inputs();
    cgi.manage_bt();
    cgi.process_cgi_data();
    cgi.generate_html();

    return 0;
}


