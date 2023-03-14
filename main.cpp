#include <fstream>
#include <iostream>
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
	playlog_parser parser(info);
	parser.parse(file);
	default_process(info);
	auto results = score(info);
	std::ofstream output("JDCscores.csv");
	output_as_csv(output, results);
	return 0;
}
