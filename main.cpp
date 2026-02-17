#include <iostream>
#include <fstream>
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <csignal>
#include <io.h>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <set>
#include <regex>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")


using namespace std;

void preciseSleep(DWORD milliseconds) {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    const LONGLONG target = (freq.QuadPart * milliseconds) / 1000LL;
    
    if (milliseconds > 2) {
        Sleep(milliseconds - 2);
    }

    do {
        QueryPerformanceCounter(&end);
    } while ((end.QuadPart - start.QuadPart) < target);
}

struct Config {
    POINT inputPos;
    POINT colorCheckPos;
    POINT colorCheckPosMenu;
    int Timer;
    string menuColor;
    string onlineColor;
};

bool stopScript = false;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        wcout << L"\nОстановка скрипта по Ctrl + C." << endl;
        stopScript = true;
    }
}

void getMouseClickPosition(POINT &pos, const wstring &message) {
    wcout << message << L" Нажмите ENTER..." << endl;
    while (!(GetAsyncKeyState(VK_RETURN) & 0x8000));
    wcin.get();
    GetCursorPos(&pos);
    wcout << L"Координаты записаны: " << pos.x << L", " << pos.y << endl;
    preciseSleep(500);
}

string colorToHex(COLORREF color) {
    char hexColor[8];
    sprintf(hexColor, "#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return string(hexColor);
}

COLORREF getPixelColor(POINT pos) {
    HDC hdc = GetDC(NULL);
    COLORREF color = 0;
    HMODULE gdi = LoadLibraryW(L"gdi32.dll");
    if (gdi) {
        typedef COLORREF(WINAPI *GetPixel_t)(HDC, int, int);
        GetPixel_t pGetPixel = (GetPixel_t)GetProcAddress(gdi, "GetPixel");
        if (pGetPixel) {
            color = pGetPixel(hdc, pos.x, pos.y);
        }
        FreeLibrary(gdi);
    }
    ReleaseDC(NULL, hdc);
    return color;
}

void saveConfig(const Config &config) {
    ofstream file("config.txt");
    file << config.inputPos.x << " " << config.inputPos.y << endl;
    file << config.colorCheckPos.x << " " << config.colorCheckPos.y << endl;
    file << config.colorCheckPosMenu.x << " " << config.colorCheckPosMenu.y << endl;
    file << config.Timer << endl;
    file << config.menuColor << endl;
    file << config.onlineColor << endl;
    file.close();
}

bool loadConfig(Config &config) {
    ifstream file("config.txt");
    if (!file.is_open()) return false;
    file >> config.inputPos.x >> config.inputPos.y;
    file >> config.colorCheckPos.x >> config.colorCheckPos.y;
    file >> config.colorCheckPosMenu.x >> config.colorCheckPosMenu.y;
    file >> config.Timer;
    file >> config.menuColor;
    file >> config.onlineColor;
    file.close();
    return true;
}

void copyClipboardToFile(const std::wstring& filename) {
    if (!OpenClipboard(nullptr)) {
        wcout << L"Ошибка: Не удалось открыть буфер обмена." << endl;
        wcin.get();
        exit(0);
    }

    HANDLE hClipboardData = GetClipboardData(CF_UNICODETEXT);
    if (hClipboardData == nullptr) {
        wcout << L"Ошибка: Буфер обмена пуст или не содержит текст." << endl;
        CloseClipboard();
        wcin.get();
        exit(0);
    }

    wchar_t* clipboardText = static_cast<wchar_t*>(GlobalLock(hClipboardData));
    if (clipboardText == nullptr) {
        wcout << L"Ошибка: Не удалось заблокировать данные из буфера обмена." << endl;
        CloseClipboard();
        wcin.get();
        exit(0);
    }

    set<wstring> processedIDs;
    wstringstream stream(clipboardText);
    wstring line;

    wregex idPattern(L"\\b\\d{3,6}\\b");

    while (getline(stream, line)) {
        line.erase(0, line.find_first_not_of(L" \t\r\n")); 
        line.erase(line.find_last_not_of(L" \t\r\n") + 1);

        if (line.empty()) continue;

        wsmatch match;
        
        if (iswalpha(line[0])) {
            size_t pos = line.find_first_of(L" \t");
            if (pos != wstring::npos) {
                wstring idPart = line.substr(pos + 1);
                if (regex_match(idPart, idPattern)) {
                    processedIDs.insert(idPart + L"+");
                }
            }
        }
        else if (regex_search(line, match, idPattern)) {
            wstring id = match.str();

            size_t pos = line.find(id);
            if (pos + id.length() < line.length()) {
                wstring remaining = line.substr(pos + id.length());
                wregex numberPattern(L"\\s+\\d+");
                if (regex_search(remaining, numberPattern)) {
                    processedIDs.insert(id + L"+"); 
                } else {
                    processedIDs.insert(id);
                }
            } else {
                processedIDs.insert(id); 
            }
        }
    }

    GlobalUnlock(hClipboardData);
    CloseClipboard();

    if (processedIDs.empty()) {
        wcout << L"Ошибка: Буфер обмена не содержит допустимых ID." << endl;
        wcin.get();
        exit(0);
    }

    vector<wstring> sortedIDs(processedIDs.begin(), processedIDs.end());

    sort(sortedIDs.rbegin(), sortedIDs.rend(), [](const wstring& a, const wstring& b) {
        int idA = std::stoi(a.substr(0, a.find_first_not_of(L"0123456789")));
        int idB = std::stoi(b.substr(0, b.find_first_not_of(L"0123456789")));
        return idA < idB; 
    });

    wofstream outFile(filename.c_str(), ios::out | ios::trunc);
    if (!outFile) {
        wcout << L"Ошибка: Не удалось открыть файл " << filename << L" для записи." << endl;
        wcin.get();
        exit(0);
    }

    for (const auto& id : sortedIDs) {
        outFile << id << endl;
    }

    outFile.close();
}

bool isProcessRunning(const std::wstring &processName) {
    HANDLE hProcessSnap;
    PROCESSENTRY32W pe32;
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return false;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(hProcessSnap, &pe32)) {
        do {
            if (std::wstring(pe32.szExeFile) == processName) {
                CloseHandle(hProcessSnap);
                return true;
            }
        } while (Process32NextW(hProcessSnap, &pe32));
    }
    CloseHandle(hProcessSnap);
    return false;
}

vector<string> readPlayerIDs(const string &filename) {
    copyClipboardToFile(L"ids.txt");
    ifstream file(filename);
    if (!file.is_open()) {
        wcout << L"Файл " << filename.c_str() << L" не найден. Создаю новый файл..." << endl;
        ofstream newFile(filename);
        newFile.close();
        wcout << L"Файл " << filename.c_str() << L" Файл успешно создан, программа будет закрыта" << endl;
        wcin.get();
        exit(0);
    }
    vector<string> ids;
    string id;
    while (getline(file, id)) {
        ids.push_back(id);
    }
    return ids;
}

void writeResults(const vector<string> &onlinePlayers) {
    ofstream file("results.txt", ios::trunc);
    for (const auto &id : onlinePlayers) {
        file << id << endl;
    }
}

void clickAtPosition(POINT pos) {
    SetCursorPos(pos.x, pos.y);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

void pasteText(const string &text) {
    OpenClipboard(0);
    EmptyClipboard();
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!hg) { CloseClipboard(); return; }
    memcpy(GlobalLock(hg), text.c_str(), text.size() + 1);
    GlobalUnlock(hg);
    SetClipboardData(CF_TEXT, hg);
    CloseClipboard();
    GlobalFree(hg);

    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('V', 0, 0, 0);
    keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
}

void clearInputField() {
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event(VK_BACK, 0, 0, 0);
    keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
}

int TimerOnWin() {
    while (true) {
        int choise;
        wcout << L"Выберите свою операционную систему: 1 - Win10, 2 - Win11: ";
        
        if (wcin >> choise) {
            if (choise == 1) {
                wcout << L"Вы выбрали Win10." << endl;
                return 30;
            } else if (choise == 2) {
                wcout << L"Вы выбрали Win11." << endl;
                return 70;
            } else {
                wcout << L"Некорректный ввод. Попробуйте еще раз..." << endl;
            }
        } else {
            wcin.clear();
            wcin.ignore(numeric_limits<streamsize>::max(), '\n');
            wcout << L"Некорректный ввод. Попробуйте еще раз..." << endl;
        }
    }
}

void ensureDpiAwareness() {
    HMODULE shcore = LoadLibraryW(L"Shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI *SetProcessDpiAwareness_t)(int);
        SetProcessDpiAwareness_t pSet = (SetProcessDpiAwareness_t)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (pSet) {
            pSet(PROCESS_PER_MONITOR_DPI_AWARE);
        }
        FreeLibrary(shcore);
    } else {
        HMODULE user = GetModuleHandleW(L"user32.dll");
        if (user) {
            typedef BOOL(WINAPI *SetProcessDPIAware_t)(void);
            SetProcessDPIAware_t pSetOld = (SetProcessDPIAware_t)GetProcAddress(user, "SetProcessDPIAware");
            if (pSetOld) pSetOld();
        }
    }
}

int main() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    
    wcout << L"\n ██▀███  ▓█████ ██▒   █▓▓█████  ███▄    █  ▄▄▄       ███▄    █ ▄▄▄█████▓" << endl;
    wcout << L"▓██ ▒ ██▒▓█   ▀▓██░   █▒▓█   ▀  ██ ▀█   █ ▒████▄     ██ ▀█   █ ▓  ██▒ ▓▒" << endl;
    wcout << L"▓██ ░▄█ ▒▒███   ▓██  █▒░▒███   ▓██  ▀█ ██▒▒██  ▀█▄  ▓██  ▀█ ██▒▒ ▓██░ ▒░" << endl;
    wcout << L"▒██▀▀█▄  ▒▓█  ▄  ▒██ █░░▒▓█  ▄ ▓██▒  ▐▌██▒░██▄▄▄▄██ ▓██▒  ▐▌██▒░ ▓██▓ ░ " << endl;
    wcout << L"░██▓ ▒██▒░▒████▒  ▒▀█░  ░▒████▒▒██░   ▓██░ ▓█   ▓██▒▒██░   ▓██░  ▒██▒ ░ " << endl;
    wcout << L"░ ▒▓ ░▒▓░░░ ▒░ ░  ░ ▐░  ░░ ▒░ ░░ ▒░   ▒ ▒  ▒▒   ▓▒█░░ ▒░   ▒ ▒   ▒ ░░   " << endl;
    wcout << L"  ░▒ ░ ▒░ ░ ░  ░  ░ ░░   ░ ░  ░░ ░░   ░ ▒░  ▒   ▒▒ ░░ ░░   ░ ▒░    ░    " << endl;
    wcout << L"  ░░   ░    ░       ░░     ░      ░   ░ ░   ░   ▒      ░   ░ ░   ░      " << endl;
    wcout << L"   ░        ░  ░     ░     ░  ░         ░       ░  ░         ░          " << endl;
    wcout << L"                    ░                                                    " << endl;

    signal(SIGINT, signalHandler);
    if (!isProcessRunning(L"GTA5.exe") && !isProcessRunning(L"GTA5_Enhanced.exe")) {
        wcout << L"Процесс GTA5.exe или GTA5_Enhanced.exe не запущен. Завершение работы." << endl;
        wcin.get();
        return 1;
    }

    Config config;
    if (!loadConfig(config)) {
        wcout << L"Нажмите ENTER для настройки конфигурации..." << endl;
        wcin.get();
        config.Timer = TimerOnWin();
        preciseSleep(1000);

        
        getMouseClickPosition(config.inputPos, L"Наведите на позицию строки ввода ID");
        getMouseClickPosition(config.colorCheckPos, L"Наведите на пустую область рядом с вашим ником (справа)");
        getMouseClickPosition(config.colorCheckPosMenu, L"Наведите на 'Не знание правил - не освобождает от ответственности'");
        wcout << L"Уберите курсор с панели и нажмите ENTER" << endl;
        wcin.ignore(numeric_limits<streamsize>::max(), '\n');
        wcin.get();
        config.menuColor = colorToHex(getPixelColor(config.colorCheckPosMenu));
        wcout << L"Цвет сохранён: " << config.menuColor.c_str() << endl;
        config.onlineColor = colorToHex(getPixelColor(config.colorCheckPos));
        wcout << L"Цвет сохранён: " << config.onlineColor.c_str() << endl;
        saveConfig(config);
        wcout << L"\nПрограмма будет закрыта. Скопируйте ID и запустите заново" << endl;
        preciseSleep(4000);
        exit(0);   
    }

    int timer = config.Timer;
    do {
        wcout << L"Конфиг загружен..." << endl;
        wcout << L"Нажмите ENTER чтобы начать" << endl;
        wcin.get();
        vector<string> playerIDs = readPlayerIDs("ids.txt");
        vector<string> onlinePlayers;
        wcout << L"ID лист успешно подгружен" << endl;
        wcout << L"Начинается проверка ID..." << endl;

        for (const auto &id : playerIDs) {
            if (stopScript) break; 

            while (colorToHex(getPixelColor(config.colorCheckPosMenu)) != config.menuColor) {
                wcout << L"Меню проверки закрыто. Ожидаем, пока вы откроете его..." << endl;
                preciseSleep(500);
                if (stopScript) break;
            }

            clickAtPosition(config.inputPos);
            pasteText(id);
            preciseSleep(timer);

            string currentColor = colorToHex(getPixelColor(config.colorCheckPos));
            if (currentColor == config.onlineColor) {
                onlinePlayers.push_back(id);
            }

            clearInputField();
        }   

        wcout << L"Игроки онлайн:" << endl;
        for (const auto &id : onlinePlayers) {
            wcout << id.c_str() << endl;
        }
        
        writeResults(onlinePlayers);
        preciseSleep(100);
        keybd_event(VK_DELETE, 0, 0, 0);
        keybd_event(VK_DELETE, 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);

        wcout << L"Проверка завершена. Результаты сохранены в results.txt" << endl;
        wcout << L"\nНажмите R, чтобы перезапустить, или пробел, чтобы закрыть приложение..." << endl;
        char choice;        
        do {
            choice = _getwch();
        } while (choice != 'r' && choice != ' ');

        if (choice == ' ') {
            break;
        }

    } while (true);
}

void ensureDpiAwareness();