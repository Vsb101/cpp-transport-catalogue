#pragma once

#include <iosfwd>
#include <string_view>

#include "transport_catalogue.h"
    
namespace stat_reader {

namespace detail {

void ParseAndPrintStat(const transport::TransportCatalogue& tansport_catalogue,
     std::string_view request,
                       std::ostream& output);

} // namespace detail

void HandleStatRequests(const transport::TransportCatalogue& transport_catalogue, 
                        std::istream& input, 
                        std::ostream& output);

} // namespace stat