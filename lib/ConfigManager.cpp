// ConfigManager Class
#include "../include/ConfigManager.hpp"

using std::string;
using std::string_view;
using std::list;
using std::filesystem::path;

ConfigManager::ConfigManager(const string& configFilePath)
{
	/*
		Check config file exists
	*/
	// Create path object
	path configPathObject = configFilePath;
	// Normalise path delimiters to the systems preferred delimiters
	configPathObject = configPathObject.make_preferred();
	// Check file exists
	if (!std::filesystem::exists(configPathObject))
	{
		// File doesn't exist, Throw error
		throw InvalidConfiguration("Could not find the config file specified");
	}


	/*
		Read config file into map<string, map<string,string>> format
	*/
	// Assign strings to represent appropriate values
	string section;
	string key;
	string value;
	// Regex matching variables
	std::smatch match;
	std::regex expression("\\[.*\\]");
	// Open config file
	std::ifstream configFile(configFilePath, std::ios::binary);
	// Readlines untill EOF
	for (string currentLine; std::getline(configFile, currentLine);)
	{
		// Remove white spaces and resize string
		currentLine = trim(currentLine);
		// Comment line or empty line, ignore
		if (currentLine[0] == ';' || currentLine == "") { continue; }
		// Section header line, update section variable
		else if (currentLine[0] == '[')
		{
			// Extract section header only 
			std::regex_search(currentLine, match, expression);
			section = match.str().substr(1, match.str().length() - 2);
			std::transform(section.begin(), section.end(), section.begin(), [](unsigned char c) { return std::tolower(c); });
		}
		// Entry line, Key and Value seperated by '='
		else
		{
			// Before '='
			key = trim(currentLine.substr(0, currentLine.find("=")));
			std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
			// After '='
			value = trim(currentLine.substr(currentLine.find("=") + 1, currentLine.length() - 1));
			// Update configMap
			set(section, key, value);
		}
	}

	configFile.close();

	/*
		Normalise all paths
	*/
	// Ensure the path delimiters are set to the system preferred
	set("input", "exon-sequences", getPath("input", "exon-sequences").make_preferred().string());
	set("input", "offtarget-sites", getPath("input", "offtarget-sites").make_preferred().string());
	set("input", "gff-annotation", getPath("input", "gff-annotation").make_preferred().string());
	set("input", "bowtie2-index", getPath("input", "bowtie2-index").make_preferred().string());
	set("output", "dir", getPath("output", "dir").make_preferred().string());
	set("sgrnascorer2", "model", getPath("sgrnascorer2", "model").make_preferred().string());
	set("bowtie2", "binary", getPath("bowtie2", "binary").make_preferred().string());
	set("rnafold", "binary", getPath("rnafold", "binary").make_preferred().string());


	/*
		Check that the config file is valid
	*/
	// Run Validate function
	int returnCode;				// Return code used to check if the binary was run successfuly

	// Check that binarys are callable

	// Check bowtie2
	returnCode = system(fmt::format("{} --version >{} 2>{}", getString("bowtie2", "binary"), nullDir, nullDir).c_str());

	if (returnCode != 0)
	{
		throw InvalidConfiguration("Could not find Bowtie2 binary");
	}

	// Check rnafold
	returnCode = system(fmt::format("{} --version >{} 2>{}", getString("rnafold", "binary"), nullDir, nullDir).c_str());

	if (returnCode != 0)
	{
		throw InvalidConfiguration("Could not find RNAFold binary");
	}


	// Check that the 'n' value for the consensus is valid
	int toolCount = getConsensusToolCount();
	if (int n = getInt("consensus", "n"); n > toolCount)
	{
		throw InvalidConfiguration(fmt::format("The consensus approach is incorrectly set. You have specified {} tools to be run but the n-value is {}. Change n to be <= {}.", toolCount, n, toolCount));
	}

	// Check that output file doesn't already exist
	// Get output dir path object
	path outputDirPathObject = getPath("output", "dir");
	if (!std::filesystem::exists(outputDirPathObject))
	{
		std::filesystem::create_directories(outputDirPathObject);
	}
	// Append output file name to output dir
	set("output", "file", (outputDirPathObject / fmt::format("{}-{}", getString("general", "name"), getString("output", "filename"))).string());
	if (std::filesystem::exists(getPath("output", "file")))
	{
		throw InvalidConfiguration(fmt::format("The output file already exists: {}.\nTo avoid loosing data, please rename your output file.", getString("output", "file")));
	}

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
		throw InvalidConfiguration("Configuration file is missing some fields!");
	}


	/*
		Generate files to process
	*/
	// Check for directory or file
	if (path inputPathObject = getPath("input", "exon-sequences"); std::filesystem::is_directory(inputPathObject))
	{
		// Iterate through directory to find all files to process
		for (const std::filesystem::directory_entry& dir_entry :
			std::filesystem::directory_iterator{ inputPathObject })
		{
			// Normalise path and add to list
			path fileToProcess = dir_entry.path();
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
	set("rnafold", "input", (outputDirPathObject / fmt::format("{}-rnafold-input.txt", getString("general", "name"))).string());
	set("rnafold", "output", (outputDirPathObject / fmt::format("{}-rnafold-output.txt", getString("general", "name"))).string());

	set("bowtie2", "input", (outputDirPathObject / fmt::format("{}-bowtie2-input.txt", getString("general", "name"))).string());
	set("bowtie2", "output", (outputDirPathObject / fmt::format("{}-bowtie2-output.txt", getString("general", "name"))).string());

	set("output", "log", (outputDirPathObject / fmt::format("{}-{}.txt", getString("general", "name"), configPathObject.stem().string())).string());
	set("output", "error", (outputDirPathObject / fmt::format("{}-{}.txt", getString("general", "name"), configPathObject.stem().string())).string());
}

int ConfigManager::getConsensusToolCount()
{
	int mm10db = getBool("consensus", "mm10db");
	int sgrnascorer2 = getBool("consensus", "sgrnascorer2");
	int chopchop = getBool("consensus", "chopchop");
	return mm10db + sgrnascorer2 + chopchop;
}

void ConfigManager::set(const string& section, const string& key, string_view value)
{
	configMap[section][key] = value;
}

int ConfigManager::getInt(const string& section, const string& key)
{
	return std::stoi(this->configMap[section][key]);
}

float ConfigManager::getFloat(const string& section, const string& key)
{
	return std::stof(this->configMap[section][key]);
}

double ConfigManager::getDouble(const string& section, const string& key)
{
	return std::stod(this->configMap[section][key]);
}

string ConfigManager::getString(const string& section, const string& key)
{
	return this->configMap[section][key];
}

const char* ConfigManager::getCString(const string& section, const string& key)
{
	return this->configMap[section][key].c_str();
}

bool ConfigManager::getBool(const string& section, const string& key)
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

path ConfigManager::getPath(const string& section, const string& key)
{
	return path(this->configMap[section][key]);
}

list<string> ConfigManager::getFilesToProcess() const
{
	return this->filesToProcess;
}
