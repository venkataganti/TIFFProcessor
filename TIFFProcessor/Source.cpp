#include <stdio.h>
#include "TiffProvider.h"

using namespace std;

void ProcessCommands(std::vector<std::string>& vargs);
std::vector<std::string> SplitString(std::string& strPages, const std::string& delimeter);
bool GetTIFFParams(TIFFParams* params);

void main(int argc, char *argv[])
{
	if (argc == 1)
	{
		cout << "Invalid arguments. Type ""TIFFProcessor -help"" for help" << endl;
		return;
	}
	else if (argc == 2)
	{
		if (!((strcmp(argv[1], "-help") == 0) || (strcmp(argv[1], "-about") == 0) || (strcmp(argv[1], "-tiffparams") == 0)))
		{
			cout << "Invalid arguments. Type ""TIFFProcessor -help"" for help." << endl;
			return;
		}
	}

	if (strcmp(argv[1], "-help") == 0)
	{
		printf("\nTIFFProcessor: Performs various operations on TIFF image files. See the list of operations provided.\n");
		printf("Note: Only one action key(operation) is allowed at a time. Mixing action keys is not permitted.\n\n");

		printf("Usage: TIFFProcessor <action key> <input file> <output file>\n");
		printf("<action key> Description:\n\n");
		printf("-merge\t\tMerge two input files to create an output file.\n");
		printf("\t\t\t	Usage: TIFFProcessor -merge input1.tiff input2.tif\n");
		printf("\t\t\t	Contents of the second input file will be appended to the first input file.\n\n");

		printf("<action key>: -rblank\t\tRemove all the blank pages from the input file\n");
		printf("\t\t\t	Usage: TIFFProcessor -rblank input.tif output.tif\n");
		printf("\t\t\t	output file is optional. If no output file is provided, operation will be performed on the input file.\n\n");

		printf("<action key>: -rpageno=p1,p2,p3,p4,p5 \tRemove the pages associated with the pages numbers p1, p2 etc.\n");
		printf("\t\t\t\t	Usage: TIFFProcessor -rpageno=2,3,4 input.tif output.tif\n");
		printf("\t\t\t\t	Multiple page numberes can be provided. Output file is optional.\n");
		printf("\t\t\t\t	If no output file is provided, operation will be performed on the input file.\n\n");

		printf("<action key>: -rgb2gray\t\tConvert coloured(RGB) pages to GRAYSCALE\n");
		printf("\t\t\t	Usage: TIFFProcessor -rgb2gray input.tif output.tif\n");
		printf("\t\t\t	output file is optional. If no output file is provided, operation will be performed on the input file.\n\n");

		printf("<action key>: -gray2binary\tConvert coloured(RGB) pages to Monochrome(Black & White or Binary)\n");
		printf("\t\t\t	Usage: TIFFProcessor -rgb2binary input.tif output.tif\n");
		printf("\t\t\t	output file is optional. If no output file is provided, operation will be performed on the input file.\n\n");

		printf("<action key>: -fileinfo\t\tDisplays basic information about all the pages in a file.\n");
		printf("\t\t\t	Usage: TIFFProcessor -fileinfo input.tif output.tif\n");
		printf("\t\t\t\tOutput file is optional. If output file is given, fileinfo will be written to the output file.\n\n");

		printf("<action key>: -tiffparams\tDisplay the values of the TIFF params from the Settings.txt.\n");
		printf("\t\t\t\tIf Settings.txt doesnt exisit or a specific TIFF param is not set in the settings.txt file, the default values are displayed.\n\n");

		printf("<action key>: -about\t\tDisplay information about the utility program, such as name, version and author\n");
		printf("<action key>: -help\t\tDisplay the help for the command line utility\n");
		return;
	}

	std::vector<std::string> vargs;
	for (int i = 1; i < argc; i++)
		vargs.emplace_back(argv[i]);

	ProcessCommands(vargs);
}

void ProcessCommands(std::vector<std::string>& vargs)
{
	bool bRes = false;
	std::string commandName = "", infile = "", infile2 = "", outfile="";

	TIFFParams tiffParams;
	CTiffProvider tifProvider;

	//Read the tiff parameters from settings.txt and set to the tiffprovider
	if (GetTIFFParams(&tiffParams))
		tifProvider.SetTIFFParams(tiffParams);

	commandName = vargs[0];

	if (vargs.size() == 1)
	{
		if (strcmp(commandName.c_str(), "-about") == 0)
		{
			printf("Utility Name:\t TIFFProcessor.exe\n");
			printf("Version:\t 1.8.2020\n");
			printf("Author:\t\t Venkata Ananth Ganti\n");
		}
		else if (commandName == "-tiffparams")
		{
			cout << "TIFF Parameters" << endl;
			cout << "---------------" << endl;
			cout << "TIFF files path set to : " << tiffParams._strFilesPath << endl;
			cout << "TIFF files compression set to : " << tiffParams._strCompressType << endl;
			cout << "TIFF binary files threshold set to : " << tiffParams._iThreshold << endl << endl;
		}
		return;
	}
	
	vargs.erase(vargs.begin() + 0);

	if (vargs.size() == 0)
	{
		cout << "Insufficient argumnets passed." << endl;
		return;
	}

	if (commandName != "-about")
	{
		infile = tiffParams._strFilesPath + vargs[0];
		(vargs.size() == 1) ? outfile = "" : (outfile = tiffParams._strFilesPath + vargs[1]);
	}

	if (commandName == "-merge")
	{
		if (vargs.size() == 1)
		{
			cout << "Insufficient argumnets passed." << endl;
			return;
		}
		bRes = tifProvider.MergeFiles(infile, outfile);
	}
	else if (commandName.find("-rpageno=", 0) != std::string::npos)
	{
		std::string strPageno = commandName.substr(9);
		std::vector<std::string> vPages = SplitString(strPageno, ",");
		std::set<uint16_t> vPagenumbers;

		//remove duplicate page numbers, if provided
		for (auto page : vPages)
		{
			vPagenumbers.emplace(std::stoi(page));
		}
		bRes = tifProvider.RemovePageByNumber(infile, vPagenumbers, outfile);
	}
	else if (commandName == "-rblank")
		bRes = tifProvider.RemoveBlankPages(infile, outfile);
	else if (commandName == "-togray")
		bRes = tifProvider.ConvertPageTo(infile, CTiffProvider::m_eConvertCode::TOGRAY, outfile);
	else if (commandName == "-tobinary")
		bRes = tifProvider.ConvertPageTo(infile, CTiffProvider::m_eConvertCode::TOBINARY, outfile);
	else if (commandName == "-fileinfo")
	{
		std::string strfileinfo = "";
		bRes = tifProvider.GetFileInfo(infile, strfileinfo, outfile);
		if(outfile.empty())
			cout << strfileinfo << endl;
	}
	else if (commandName == "-tiffparams")
	{
		cout << "TIFF Parameters" << endl;
		cout << "---------------" << endl;
		cout << "TIFF files path set to : " << tiffParams._strFilesPath << endl;
		cout << "TIFF files compression set to : " << tiffParams._strCompressType << endl;
		cout << "TIFF binary files threshold set to : " << tiffParams._iThreshold << endl << endl;
	}
	else
	{
		cout << "Invalid command key!!" << endl;
		return;
	}

	(bRes == true) ? cout << "Operation sucessful!!" << endl : cout << tifProvider.GetErrorMsg().c_str() << endl;
}

bool GetTIFFParams(TIFFParams* params)
{
	char filedata[1024];
	std::string strLine = "";
	FILE* fp = nullptr;

	fopen_s(&fp, "settings.txt", "r");

	if (fp == nullptr)
	{
		cout << "Error opening Settings.txt" << endl;
		return false;
	}

	while (!feof(fp))
	{
		fgets(filedata, 1024, fp);
		strLine = filedata;
		
		//remove \n from the end
		strLine.erase(std::remove(strLine.begin(), strLine.end(), '\n'), strLine.end());
		std::vector<std::string> vParams = SplitString(strLine, "=");

		if (vParams.size() == 2)
		{
			if (vParams[0] == "imagefilespath")
			{
				params->_strFilesPath = vParams[1];
				std::size_t found = params->_strFilesPath.find_last_of("\\");
				if (found != params->_strFilesPath.length()-1)
					params->_strFilesPath.append("\\");
			}
			if (vParams[0] == "compression")
			{
				params->_strCompressType = vParams[1];
			}
			if (vParams[0] == "threshold")
			{
				params->_iThreshold = std::stoi(vParams[1]);
			}
		}
	}
	fclose(fp);

	return true;
}

std::vector<std::string> SplitString(std::string& strPages, const ::string& delimeter)
{

	std::vector<std::string> vstrings;
	std::string::size_type found;
	std::string strChoppedString = strPages;
	std::string str;

	do
	{
		found = strChoppedString.find(delimeter);
		str = strChoppedString.substr(0, found);
		vstrings.push_back(str);
		strChoppedString = strChoppedString.substr(found + 1);
	} while (found != std::string::npos);

	return vstrings;
}
