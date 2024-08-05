#include <charconv>
#include <fstream>
#include <iostream>
#include <string>
#include "match.h"
#include "output.h"
#include "playlog.h"
#include "scoring.h"

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cerr << "ERROR: the program expects exactly 1 argument (playlog filename)\n";
		return 1;
	}
	std::ifstream file(argv[1]);
	event_data info;
	{
		std::cout << "Set max score (default 100): " << std::flush;
		std::string str;
		std::getline(std::cin, str);
		double value;
		auto result = std::from_chars(str.data(), str.data() + str.size(), value);
		if (result.ec == std::errc {})
			info.max_score = value;
	}
	playlog_parser parser(info);
	parser.parse(file);
	default_process(info);
	auto results = score(info);
	std::ofstream output("JDCscores.csv");
	output_as_csv(output, results);
	return 0;
}
