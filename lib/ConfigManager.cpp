// ConfigManager Class
#include <ConfigManager.hpp>

using std::string;
using std::list;

ConfigManager::ConfigManager(string configFilePath) 
{
	/*
		Check config file exists
	*/ 
	// Create path object
	std::filesystem::path configPathObject = configFilePath;
	// Normalise path delimiters to the systems preferred delimiters
	configPathObject = configPathObject.make_preferred();
	// Check file exists
	if (!std::filesystem::exists(configPathObject))
	{
		// File doesn't exist, Throw error
		throw std::runtime_error("Could not find the config file specified");
	}


	/*
		Read config file into map<string, map<string,string>> format
	*/
	// Assign strings to represent appropriate values
	string section, key, value, currentLine;
	// Regex matching variables
	std::smatch match;
	std::regex expression("\\[.*\\]");
	// Open config file
	std::ifstream configFile(configFilePath);
	// Readlines untill EOF
	while (std::getline(configFile, currentLine))
	{
		// Remove white spaces and resize string
		currentLine.erase(std::remove(currentLine.begin(), currentLine.end(), ' '), currentLine.end());
		// Comment line or empty line, ignore
		if (currentLine[0] == ';' || currentLine == "") { continue; }
		// Section header line, update section variable
		else if (currentLine[0] == '[')
		{
			// Extract section header only 
			std::regex_search(currentLine, match, expression);
			section = match.str().substr(1, match.str().length() - 2);
		}
		// Entry line, Key and Value seperated by '='
		else
		{
			// Before '='
			key = currentLine.substr(0, currentLine.find("="));
			// After '='
			value = currentLine.substr(currentLine.find("=")+1, currentLine.length()-1);
			// Update configMap
			set(section, key, value);
		}
	}


	/*
		Normalise all path
	*/
	// Ensure the path delimiters are set to the system preferred
	set("input", "exon-seqeunces", getPath("input", "exon-seqeunces").make_preferred().string());
	set("input", "offtarget-site", getPath("input", "offtarget-site").make_preferred().string());
	set("input", "gff-annotation", getPath("input", "gff-annotation").make_preferred().string());
	set("input", "bowtie2-index", getPath("input", "bowtie2-index").make_preferred().string());
	set("output", "dir", getPath("output", "dir").make_preferred().string());
	set("offtargetscore", "binary", getPath("offtargetscore", "binary").make_preferred().string());
	set("sgrnascorer2", "model", getPath("sgrnascorer2", "model").make_preferred().string());
	set("bowtie2", "binary", getPath("bowtie2", "binary").make_preferred().string());
	set("rnafold", "binary", getPath("rnafold", "binary").make_preferred().string());


	/*
		Check that the config file is valid
	*/
	// Run Validate function
	char buffer[1024];		// Buffer used to format strings
	int returnCode;			// Return code used to check if the binary was run successfuly

	// Check that binarys are callable

	// Check ISSL
	snprintf(buffer, 1024, "%s --version", getCString("offtargetscore", "binary"));
	returnCode = system(buffer);
	if (returnCode != 0)
	{
		std::cout << "Could not run ISSL BINARY" << "\n";
	}

	// Check bowtie2
	snprintf(buffer, 1024, "%s --version", getCString("bowtie2", "binary"));
	returnCode = system(buffer);
	if (returnCode != 0)
	{
		std::cout << "Could not run bowtie2 BINARY" << "\n";
	}

	// Check rnafold
	snprintf(buffer, 1024, "%s --version", getCString("rnafold", "binary"));
	returnCode = system(buffer);
	if (returnCode != 0)
	{
		std::cout << "Could not run RNAfold BINARY" << "\n";
	}

	// Check that the 'n' value for the consensus is valid
	int toolCount = getConsensusToolCount();
	int n = getInt("consensus","n");
	if (n > toolCount)
	{
		snprintf(buffer, 1024, "The consensus approach is incorrectly set. You have specified %d tools to be run but the n-value is %d. Change n to be <= %d.", toolCount, n, toolCount);
		std::cout << buffer << "\n";
	}

	// Check that output file doesn't already exist
	// Generate outputfile name
	snprintf(buffer, 1024, "%s-%s", getCString("general", "name"), getCString("output", "filename"));
	// Get output dir path object
	std::filesystem::path outputDirPathObject = getPath("output", "dir");
	// Append output file name to output dir
	set("output", "file", (outputDirPathObject / buffer).string());
	if (std::filesystem::exists(getPath("output","file")))
	{
		snprintf(buffer, 1024, "The output file already exists: %s.\nTo avoid loosing data, please rename your output file.", getCString("output", "file"));
		std::cout << buffer << "\n";
	};

	// Check all the fields have values
	if (
		(getString("general", "name") == "") || 
		(getString("general", "optimisation") == "") ||
		(getString("consensus", "n") == "") ||
		(getString("consensus", "mm10db") == "") ||
		(getString("consensus", "sgrnascorer2") == "") ||
		(getString("consensus", "chopchop") == "") ||
		(getString("input", "exon-sequences") == "") ||
		(getString("input", "offtarget-sites") == "") ||
		(getString("input", "gff-annotation") == "") ||
		(getString("input", "bowtie2-index") == "") ||
		(getString("input", "batch-size") == "") ||
		(getString("output", "dir") == "") ||
		(getString("output", "filename") == "") ||
		(getString("output", "delimiter") == "") ||
		(getString("offtargetscore", "enabled") == "") ||
		(getString("offtargetscore", "binary") == "") ||
		(getString("offtargetscore", "method") == "") ||
		(getString("offtargetscore", "threads") == "") ||
		(getString("offtargetscore", "page-length") == "") ||
		(getString("offtargetscore", "score-threshold") == "") ||
		(getString("offtargetscore", "max-distance") == "") ||
		(getString("sgrnascorer2", "model") == "") ||
		(getString("sgrnascorer2", "score-threshold") == "") ||
		(getString("bowtie2", "binary") == "") ||
		(getString("bowtie2", "threads") == "") ||
		(getString("bowtie2", "page-length") == "") ||
		(getString("rnafold", "binary") == "") ||
		(getString("rnafold", "threads") == "") ||
		(getString("rnafold", "page-length") == "") ||
		(getString("rnafold", "low_energy_threshold") == "") ||
		(getString("rnafold", "high_energy_threshold") == "")
	)
	{
		throw std::runtime_error("Configuration file is missing some fields!");
	}


	/*
		Generate files to process
	*/
	// Create path object for input
	std::filesystem::path inputPathObject = getPath("input","exon-sequences");
	// Check for directory or file
	if (std::filesystem::is_directory(inputPathObject))
	{
		// Iterate through directory to find all files to process
		for (const std::filesystem::directory_entry& dir_entry :
			std::filesystem::directory_iterator{ inputPathObject })
		{
			// Normalise path and add to list
			std::filesystem::path fileToProcess = dir_entry.path();
			filesToProcess.push_back(fileToProcess.make_preferred().string());
		}
	}
	else if (std::filesystem::is_regular_file(inputPathObject))
	{
		// Only one file to process
		filesToProcess.push_back(inputPathObject.string());
	}

	/*
		Generate temp output file names
	*/
	snprintf(buffer, 1024, "%s-rnafold-input.txt", getCString("general", "name"));
	set("rnafold","input", (outputDirPathObject / buffer).string());

	snprintf(buffer, 1024, "%s-rnafold-output.txt", getCString("general", "name"));
	set("rnafold", "output", (outputDirPathObject / buffer).string());

	snprintf(buffer, 1024, "%s-offtargetscore-input.txt", getCString("general", "name"));
	set("offtargetscore", "input", (outputDirPathObject / buffer).string());

	snprintf(buffer, 1024, "%s-offtargetscore-output.txt", getCString("general", "name"));
	set("offtargetscore", "output", (outputDirPathObject / buffer).string());

	snprintf(buffer, 1024, "%s-bowtie-input.txt", getCString("general", "name"));
	set("bowtie2", "input", (outputDirPathObject / buffer).string());

	snprintf(buffer, 1024, "%s-bowtie-output.txt", getCString("general", "name"));
	set("bowtie2", "output", (outputDirPathObject / buffer).string());

	snprintf(buffer, 1024, "%s-%s.log", getCString("general", "name"), configPathObject.stem().string().c_str());
	set("output", "log", (outputDirPathObject / buffer).string());

	snprintf(buffer, 1024, "%s-%s.errlog", getCString("general", "name"), configPathObject.stem().string().c_str());
	set("output", "error", (outputDirPathObject / buffer).string());
}

int ConfigManager::getConsensusToolCount()
{
	bool mm10db = getBool("consensus", "mm10db");
	bool sgrnascorer2 = getBool("consensus", "sgrnascorer2");
	bool chopchop = getBool("consensus", "chopchop");
	return mm10db+sgrnascorer2+chopchop;
}

void ConfigManager::set(string section, string key, string value)
{
	configMap[section][key] = value;
}

int ConfigManager::getInt(string section, string key)
{
	return std::stoi(this->configMap[section][key]);
}

float ConfigManager::getFloat(string section, string key)
{
	return std::stof(this->configMap[section][key]);
}

double ConfigManager::getDouble(string section, string key)
{
	return std::stod(this->configMap[section][key]);
}

string ConfigManager::getString(string section, string key)
{
	return this->configMap[section][key];
}

const char* ConfigManager::getCString(string section, string key) {
	return this->configMap[section][key].c_str();
}

bool ConfigManager::getBool(string section, string key)
{
	string boolValue = this->configMap[section][key];
	std::transform(boolValue.begin(), boolValue.end(), boolValue.begin(), [](unsigned char c) { return std::tolower(c); });
	if (boolValue == "true") { return true; }
	else if (boolValue == "false") { return false; }
	else 
	{ 
		throw std::invalid_argument("The value selected is not of the type bool!");
	}
}

std::filesystem::path ConfigManager::getPath(string section, string key)
{
	return std::filesystem::path(this->configMap[section][key]);
}

list<string> ConfigManager::getFilesToProcess() {
	return this->filesToProcess;
}