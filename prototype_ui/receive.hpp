#pragma once

#include "common.hpp"

#include <vector>
#include <thread>

// Return latest available data
std::vector<radar_data_t> give_data();

// Receive data thread
void receive_task(std::stop_token stop);
