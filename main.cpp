#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif 
#include "bcm2835.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include <random>
#include <sstream> 
#include <iomanip>
#include <thread>
#include <stdio.h>
#include "civetweb.h"
#include <fstream>
#include <sys/reboot.h>
#include <sys/ioctl.h>
#include <net/if.h>

using namespace std;

#define FEVPIN RPI_BPLUS_GPIO_J8_40
#define SANPIN RPI_BPLUS_GPIO_J8_38
#define DEFAULT_BUFLEN 512
#define SERVER_PORT 8150
#define SETTINGS_SIZE 5

struct mg_context* ctx;
int serverSocket = 8150;
int connectResult;
bool bSettingsChanged = false;

static const char* html_form =
"<html><body>Reader"
"<form method=\"POST\" action=\"/handle_post_request\">"
"Server IP-Address: <input type=\"text\" name=\"input_1\" /> <br/>"
"First Pin: <input type=\"text\" name=\"input_2\" /> <br/>"
"Second Pin: <input type=\"text\" name=\"input_3\" /> <br/>"
"Delay Time: <input type=\"text\" name=\"input_4\" /> <br/>"
"Read Time: <input type=\"text\" name=\"input_5\" /> <br/>"
"<input type=\"submit\" />"
"</form></body></html>";

/*void stopWebServer()
{
    delay(5000);
    mg_stop(ctx);
    //close(serverSocket);
    //sync();
    //reboot(RB_AUTOBOOT);
}*/

int writeToConfig(vector <char*> dataArray)
{
	std::ofstream config{ "config.txt" };
	if (config.is_open())
	{
		for (int i = 0; i < dataArray.size(); i++)
		{
			if (i == dataArray.size() - 1)
			{
				config << dataArray[i];
			}
			else
				config << dataArray[i] << endl;
		}

		config.close();
		return 0;
	}
	else return -1;
}

static int begin_request_handler(struct mg_connection* conn)
{
    const struct mg_request_info* ri = mg_get_request_info(conn);
    char post_data[1024], serverIp[sizeof(post_data)], fev[sizeof(post_data)], san[sizeof(post_data)], delayt[sizeof(post_data)], readt[sizeof(post_data)];
    int post_data_len;

    if (!strcmp(ri->local_uri, "/handle_post_request")) {
        // User has submitted a form, show submitted data and a variable value
        post_data_len = mg_read(conn, post_data, sizeof(post_data));

        // Parse form data. input1 and input2 are guaranteed to be NUL-terminated
        mg_get_var(post_data, post_data_len, "input_1", serverIp, sizeof(serverIp));
        mg_get_var(post_data, post_data_len, "input_2", fever, sizeof(fev));
	mg_get_var(post_data, post_data_len, "input_3", sanitizer, sizeof(san));
	mg_get_var(post_data, post_data_len, "input_4", delayt, sizeof(delayt));
	mg_get_var(post_data, post_data_len, "input_5", readt, sizeof(readt));

        // Send reply to the client, showing submitted form values.
        mg_printf(conn, "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "Submitted data: [%.*s]\n"
            "Submitted data length: %d bytes\n"
            "input_1: [%s]\n",
            //"input_2: [%s]\n",
            post_data_len, post_data, post_data_len, serverIp /* , deviceIp */ );

		while (1)
		{
			vector <char*> dataArray = { serverIp, fev, san, delayt, readt };
			int writeResult = writeToConfig(dataArray);
			if (writeResult == 0) break;
		}
		bSettingsChanged = true;

        //thread stopThread(stopWebServer);
        //stopThread.detach();
    }
    else {
        // Show HTML form.
        mg_printf(conn, "HTTP/1.0 200 OK\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: text/html\r\n\r\n%s",
            (int)strlen(html_form), html_form);
    }
    return 1;  // Mark request as processed
}

void startWebServer()
{
    const char* options[] = {"listening_ports", "8080", NULL};
    struct mg_callbacks callbacks;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = begin_request_handler;
    ctx = mg_start(&callbacks, NULL, options);
    //getchar();  // Wait until user hits "enter"
    //mg_stop(ctx);
}

void readPin(int& in, int pin, string msg) {
    in = unsigned(bcm2835_gpio_lev(pin));
    cout << msg << in << endl;
}

void delayfor(int ms) {
#ifdef WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

int main()
{
	/* This is our Web Server thread, which is going to run independently
	   and provide an User Interface for reading and writing to created config.txt,
	   which will be in the client.out file folder and contain all settings. 
	   To open this interface, please type IP:Port to a web broser. Example:
	   RPi IP Address: 192.168.2.104, Port: 8080 => 192.168.2.104:8080
	   For this Web Server, we are using Civetweb OpenSource Library.
	*/
	thread startThread(startWebServer);
	startThread.detach();

	vector <string> settingsStr;
	while (1)
	{
		bSettingsChanged = false;
		if (bSettingsChanged) continue;

		// Step 1: Getting Settings from the config.txt
		vector <char*> settings;
		settingsStr.clear();
		ifstream config("config.txt");
		if (config.is_open())
		{
			string line;
			while (getline(config, line))
			{
				cout << line << '\n';
				settingsStr.push_back(line);
			}
			config.close();
			cout << endl;

			char* str = nullptr;
			for (int i = 0; i < settingsStr.size(); i++)
			{
				str = new char[settingsStr[i].length() + 1];
				settings.push_back(str);
				strcpy(settings[i], settingsStr[i].c_str());
			}
		}
		else
			cout << "config cant be opened to read from" << endl;

		cout << endl << "Settings: " << endl;
		for (int i = 0; i < settings.size(); i++)
		{
			cout << settings[i] << endl;
		}

		// Step 2.1: Creating a socket for server communication
		while (1)
		{
			if (bSettingsChanged) break;
			serverSocket = socket(AF_INET, SOCK_STREAM, 0);
			printf("serverSocket: %d\n", serverSocket);
			if (serverSocket <= 0)
			{
				delayfor(1000);
				continue;
			}
			else break;
		}
		if (bSettingsChanged || settings.size() != SETTINGS_SIZE)
		{
			close(serverSocket);
			for (int i = 0; i < settings.size(); i++)
				delete[] settings[i];
			continue;
		}

		// Step 2.2: Connecting to the server socket
		bool bDoneOnce = false;
		struct sockaddr_in server;
		while (1)
		{
			if (bSettingsChanged) break;
			unsigned long addr;
			memset(&server, 0, sizeof(server));
			addr = inet_addr(settings[0]);
			memcpy((char*)&server.sin_addr, &addr, sizeof(addr));
			server.sin_family = AF_INET;
			server.sin_port = htons(SERVER_PORT);
			struct timeval timeout;
			timeout.tv_sec = 1;  // after 1 second connect() will timeout
			timeout.tv_usec = 0;
			setsockopt(serverSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
			connectResult = connect(serverSocket, (struct sockaddr*)&server, sizeof(server));
			delay(500);
			if (connectResult == 0)
			{
				cout << "connectResult: " << connectResult << endl;
				break;
			}
			// I just want -1 to show once, otherwise it would flood the screen
			else if (connectResult == -1 && !bDoneOnce)
			{
				bDoneOnce = true;
				cout << "connectResult: " << connectResult << endl;
			}
		}
		if (bSettingsChanged)
		{
			close(serverSocket);
			for (int i = 0; i < settings.size(); i++)
				delete[] settings[i];
			continue;
		}

		// Check Settings
		if (settings.size() != SETTINGS_SIZE)
		{
			close(serverSocket);
			for (int i = 0; i < settings.size(); i++)
				delete[] settings[i];
			continue;
		}

		bcm2835_init();
		bcm2835_gpio_fsel(FEVPIN, BCM2835_GPIO_FSEL_INPT);
		bcm2835_gpio_fsel(SANPIN, BCM2835_GPIO_FSEL_INPT);

		while (1) {
			if (bSettingsChanged) break;

			const int fevSet = stoi(settings[1]);
			const int sanSet = stoi(settings[2]);
			const int readt = stoi(settings[4]);
			const int dt = 500;
			int fevIn = 0;
			int sanIn = 0;
			bool bSend = false;

			// Step 3.1: read pins and set sending condition
			if (fevSet == 1 && sanSet == 1) {
				readPin(fevIn, FEVPIN, "FEVPIN: ");
				readPin(sanIn, SANPIN, "SANPIN: ")
				if (fevIn == 1) {
					for (int i = 0; i < readt / dt; i++ ) {
						readPin(sanIn, SANPIN, "SANPIN: ");
						if (sanIn == 1) {
							bSend = true;
							break;
						}
						delayfor(dt);
					}
				}
				else if (sanIn == 1) {
					for (int i = 0; i < readt / dt; i++ ) {
						readPin(fevIn, FEVPIN, "FEVPIN: ");
						if (fevIn == 1) {
							bSend = true;
							break;
						}
						delayfor(dt);
					}
				}
			}
			else if (fevSet == 1 && sanSet == 0) {
				readPin(fevIn, FEVPIN, "FEVPIN: ");
				if (fevIn == 1)
					bSend = true;
			}
			else if (fevSet == 0 && sanSet == 1) {
				readPin(sanIn, SANPIN, "SANPIN: ");
				if (sanIn == 1)
					bSend = true;
			}
			
			
			// Step 3.2: send command
			if (bSend == true) {
				char commandt[1] = { 0xAA };
				char commandf[1] = { 0xBB };

				int sendResult = send(serverSocket, commandt, 1, 0);
				printf("sendResult: %d\n", sendResult);
				int delayTime = stoi(settings[3]);
				delayfor(delayTime);
				sendResult = send(serverSocket, commandf, 1, 0);
				printf("sendResult: %d\n", sendResult);
			}
			else
				delayfor(dt);
		}
		if (bSettingsChanged) {
			close(serverSocket);
			for (int i = 0; i < settings.size(); i++)
				delete[] settings[i];
			continue;
		}
	}

	return 0;
}
