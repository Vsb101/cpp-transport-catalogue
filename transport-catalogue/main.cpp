#include "json_reader.h"
#include "transport_router.h"
#include "request_handler.h"
#include "map_renderer.h"

#include <iostream>

int main() {
    try {
        TransportCatalogue catalogue;
        JsonReader reader;

        // === Читаем JSON ===
        reader.ReadData(std::cin);

        // ===  Настраиваем рендерер ===
        renderer::Settings render_settings;
        reader.ProcessRenderSettings(render_settings);
        renderer::MapRenderer renderer(render_settings);

        // === Загружаем базу: остановки, маршруты, расстояния ===
        reader.ProcessBaseRequests(catalogue, renderer);

        // === Читаем настройки маршрутизации ===
        RoutingSettings routing_settings = reader.ReadRoutingSettings();

        // === Создаём TransportRouter через Builder ===
        TransportRouter router = TransportRouterBuilder{}
        .Build(catalogue, routing_settings);


        // === Создаём обработчик с полным набором данных ===
        RequestHandler handler(catalogue, renderer, router);

        // === Обрабатываем стат-запросы ===
        reader.ProcessStatRequests(handler, std::cout);

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
