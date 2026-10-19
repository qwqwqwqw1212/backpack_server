#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <string>
#include <iterator>

using namespace std;
//
class KnapsackSolver
{
public:
    struct Result
    {
        int max_value;
        vector<int> selected;

        string toJSON()
        {
            stringstream ss;
            ss << "{\"max_value\":" << max_value << "\",\"selected\":[";
            for (size_t i = 0; i < selected.size(); i++)
            {
                if (i > 0)
                    ss << ",";
                ss << selected[i];
            }
            ss << "]}";
            return ss.str();
        }
    };
    static Result solve(const vector<int> &weights, const vector<int> &values, int capacity)
    {
        int n = weights.size();
        vector<vector<int>> dp(n + 1, vector<int>(capacity + 1, 0));
        for (int i = 1; i <= n; i++)
        {
            for (int w = 0; w <= capacity; w++)
            {
                if (weights[i - 1] <= w)
                {
                    dp[i][w] = max(dp[i - 1][w], dp[i - 1][w - weights[i - 1]] + values[i - 1]);
                }
                else
                {
                    dp[i][w] = dp[i - 1][w];
                }
            }
        }
        vector<int> selected;
        int w = capacity;
        for (int i = n; i > 0 && w > 0; i--)
        {
            if (dp[i][w] != dp[i - 1][w])
            {
                selected.push_back(i - 1);
                w -= weights[i - 1];
            }
        }
        reverse(selected.begin(), selected.end());
        return {dp[n][capacity], selected};
    }
}
// HTTP 请求解析器
#include <map>
#include <regex>
#include <cstring>

class HTTPParser
{
public:
    struct Request
    {
        string method;              // get/post 方法
        string path;                // 请求路径
        map<string, string> params; // URL 参数 ：
    };
    static bool parse(const string &raw, Request &req)
    { // 解析第一行请求
        size_t pos = raw.find('\n');
        if (pos == string::npos)
            return false;
        string first_line = raw.substr(0, pos);
        stringstream line_stream(first_line);
        string version;
        // 解析: GET /?weight=10,20&value=60,100&capacity=50 HTTP/1.1
        if (!(line_stream >> req.method >> req.path >> version))
        {
            return false;
        }
        size_t q_pos = req.path.find('?');
        if (q_pos != string::npos)
        {
            string query = req.path.substr(q_pos + 1);
            req.path = req.path.substr(0, q_pos);

            stringstream query_stream(query); // 初始化为query的内容
            string param;
            while (getline(query_stream, param, '&'))
            {
                size_t eq_pos = param.find('=');
                if (eq_pos != string::npos)
                {
                    string key = param.substr(0, eq_pos);
                    string value = param.substr(eq_pos + 1);
                    req.params[key] = value;
                }
            }
        }
        return true;
    }
};

// HTTP响应器
class ResponsGenerator
{
public:
    // 200ok;
    static string success(const string &json)
    {
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: " +
               to_string(json.length()) + "\r\n"
                                          "Connection: close\r\n"
                                          "\r\n" +
               json;
    }
    // 400 bad request
    static string error(const string &msg)
    {
        string json = "{\"error\":\"" + msg + "\"}";
        return "HTTP/1.1 400 Bad Request\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: " +
               to_string(json.length()) + "\r\n"
                                          "Connection: close\r\n"
                                          "\r\n" +
               json;
    }
};

#include <unistd.h>
#include <sys/socket.h>

void handleClient(int client_fd)
{
    char buffer[4069] = {0};
    int bytes = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes <= 0)
    {
        close(client_fd);
        return;
    }
    string request_str(buffer);
    HTTPParser::Request req;

    // 解析请求
    if (!HTTPParser::parse(request_str, req))
    {
        string resp = ResponsGenerator::error("Invalid HTTP request");
        send(client_fd, resp.c_str(), resp.length(), 0);
        close(client_fd);
        return;
    }
    // 只支持get方法
    if (req.method != "GET")
    {
        string resp = ResponsGenerator::error("Only GET method supported");
        send(client_fd, resp.c_str(), resp.length(), 0);
        close(client_fd);
        return;
    }

    try
    {
        vector<int> weights, values;
        int capacity = 0;

        // 解析weight参数:"10，20，30"
        if (req.params.count("weight"))
        {
            string w_str = req.params["weight"];
            regex comma(",");
            sregex_token_iterator it(w_str.begin(), w_str.end(), comma, -1);
            sregex_token_iterator end;
            for (; it != end; ++it)
            {
                weights.push_back(stoi(*it));
            }
        }
        // 解析value参数:"60,100,120"
        if (req.params.count("value"))
        {
            string v_str = req.params["value"];
            regex comma(",");
            sregex_token_iterator it(v_str.begin(), v_str.end(), comma, -1);
            sregex_token_iterator end;
            for (; it != end; ++it)
            {
                values.push_back(stoi(*it));
            }
        }
        // 解析capacity参数:"50 "
        if (req.params.count("capacity"))
        {
            capacity = stoi(req.params["capacity"]);
        }
        // 参数验证
        if (weights.empty() || values.empty() || capacity <= 0)
        {
            throw runtime_error("Missing parameters: weight, value,capacity required");
        }
        if (weights.size() != values.size())
        {
            throw runtime_error("Weight and value arrays must have same length");
        }

        // 调用背包求解器
        KnapsackSolver::Result result = KnapsackSolver::solve(weights, values, capacity);
        // 返回结果
        string response = ResponsGenerator::success(result.toJSON());
        send(client_fd, response.c_str(), response.length(), 0);
    }
    catch (const exception &e)
    {
        string response = ResponsGenerator::error(e.what());
        send(client_fd, response.c_str(), response.length(), 0);
    }
    close(client_fd);
}

#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 8080

// 回收子进程
void sigchld_handler(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // 设置信号处理
    signal(SIGCHLD, sigchld_handler);
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        perror("socket");
        exit(1);
    }
    // 绑定地址
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        exit(1);
    }
    // 监听
    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        exit(1);
    }
    cout << "========================================" << endl;
    cout << " 背包服务器已启动!" << endl;
    cout << " 监听地址: http://localhost:" << PORT << endl;
    cout << " 测试命令:" << endl;
    cout << "   curl \"http://localhost:" << PORT << "/?weight=10,20,30&value=60,100,120&capacity=50\"" << endl;
    cout << "========================================" << endl;

    // 主循环
    while (true)
    {

        struct sockaddr_in client_addr;
        socklen_t client_addrlen = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addrlen);
        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }
        // fork子进程处理
        pid_t pid = fork();
        if (pid == 0)
        {
            // 子进程
            close(server_fd);
            handleClient(client_fd);
            exit(0);
        }
        else if (pid > 0)
        {
            // 父进程
            close(client_fd);
        }
        else
        {
            perror("fork");
            close(client_fd);
        }
    }
    close(server_fd);
    return 0;
}