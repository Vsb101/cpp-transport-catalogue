#pragma once
#include <string>
#include <iostream>
#include <string_view>
#include <vector>

#include "geo.h"
#include "transport_catalogue.h"

namespace input {

struct CommandDescription {
    // Определяет, задана ли команда (поле command непустое)
    explicit operator bool() const {
        return !command.empty();
    }

    bool operator!() const {
        return !operator bool();
    }

    std::string command;      // Название команды
    std::string id;           // id маршрута или остановки
    std::string description;  // Параметры команды
};

class InputReader {
public:
    InputReader(transport::TransportCatalogue& catalog) : catalogue_(catalog) {};

    // Наполняет данными транспортный справочник, используя команды из commands_
    void ApplyCommands(transport::TransportCatalogue& catalogue);

    // Для чтения базовых запросов и их обработки
    void HandleBaseRequests(std::istream& input_stream);

private:
    //Парсит строку в структуру CommandDescription и сохраняет результат в commands_
    void ParseLine(std::string_view line);

    std::vector<CommandDescription> commands_;
    transport::TransportCatalogue& catalogue_;
};

} //namespace input