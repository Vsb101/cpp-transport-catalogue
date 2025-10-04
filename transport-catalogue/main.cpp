#include <iostream>
#include <string>

#include "input_reader.h"
#include "stat_reader.h"
//#include "transport_catalogue_test.h"

using namespace transport;
using namespace input;
using namespace stat_reader;

int main() {
    //transport::TransportCatalogueTest test;
    //test.RunAllTests();

    TransportCatalogue catalogue;

    InputReader reader(catalogue);
    reader.HandleBaseRequests(std::cin);
    reader.ApplyCommands(catalogue);

    HandleStatRequests(catalogue, std::cin, std::cout);
}