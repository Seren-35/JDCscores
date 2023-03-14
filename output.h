#pragma once
#include <ostream>
#include "scoring.h"

std::string sanitized_csv_string(std::string_view sv);

void output_as_csv(std::ostream& os, const scoring_results& results);
