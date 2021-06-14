#pragma once
#include "tiffio.h"
#include <string>
#include <set>
#include <map>
#include <iostream>
#include <vector>
#include <algorithm>
#include <Windows.h>
#include <filesystem>
//#include <experimental/filesystem>

using namespace std;

//this structure contains the information about each tiff page
typedef struct Header
{
	uint32_t _width;
	uint32_t _height;
	uint16_t _samplesperpixel;
	uint16_t _bitspersample;
	uint16_t _config;
	uint16_t _orientation;
	uint16_t _photometric;
	uint16_t _compression;
}TagHeader;

//we can add more tiff parameters as we add features to this.
typedef struct Params
{
	std::string _strFilesPath = "";
	std::string _strCompressType = "JPEG";
	uint16_t _iThreshold = 100;
}TIFFParams;

class CTiffProvider
{
private:
	bool m_bUseTempOutfile;
	bool m_bToGrayScale;
	bool m_bToBinary;

	std::string m_strErrorMsg = "";
	std::string m_strInputFile, m_strOutputFile;
	std::map<std::string, uint16_t> m_CompressionTypes;
	
	TagHeader m_TagHeader;
	TIFFParams m_Params;

public:
	typedef enum ConvertCode { TOBINARY = 0, TOGRAY = 1 } m_eConvertCode;
	typedef enum PageType { BINARY = 1, GRAYSCALE, COLOUR, BLANK } m_ePageType;

private:
	void GetTagInfo(TIFF* pFile);
	bool OpenIOFiles(TIFF** pInfile, TIFF** pOutfile, bool bDeleteOutputFile = true);
	void CloseIOFiles(TIFF** pInfile, TIFF** pOutfile);
	uint16_t GetPageCount(TIFF* pfile);
	int16_t WriteHeader(TIFF* pfile, TagHeader& header);
	bool ValidPixelFormat();
	bool IsPageType(TIFF* pFile, m_ePageType pType);
	bool WriteData(TIFF* pInfile, TIFF* pOutfile);
	bool ToGrayScale(unsigned char* pSourceImage, uint32_t lineSize, int iSamplesperpixel, bool ToBinary = false, int iThreshold = 0);

public:
	CTiffProvider() = default;
	CTiffProvider(TIFFParams& Params);
	~CTiffProvider();

	//avoid copying of this objects
	CTiffProvider(const CTiffProvider& second) = delete;
	
	//Helper functions
	std::string GetErrorMsg();
	TIFFParams& GetTIFFParams();
	void SetTIFFParams(TIFFParams& tiffParams);

	//Required operations
	bool MergeFiles(std::string& infile1, std::string& infile2);
	bool RemoveBlankPages(std::string& infile, std::string outfile = "");
	bool RemovePageByNumber(std::string& infile, std::set<uint16_t>& pages, std::string outfile = "");

	//Miscellaneous operations
	bool GetFileInfo(std::string& infile, std::string& fileinfo, std::string outfile = "");
	bool ConvertPageTo(std::string& infile, m_eConvertCode ccode, std::string outfile = "" );
};

