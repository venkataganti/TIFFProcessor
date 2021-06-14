#include "TiffProvider.h"



CTiffProvider::CTiffProvider(TIFFParams& Params) : m_strInputFile(""), m_strOutputFile(""), m_bUseTempOutfile(false),
												   m_bToGrayScale(false), m_bToBinary(false)
{
	m_Params = Params;
	m_CompressionTypes["NONE"] = 1;
	m_CompressionTypes["JPEG"] = 2;
	m_CompressionTypes["LZW"] = 3;
}

CTiffProvider::~CTiffProvider()
{

}

//PRIVATE MEMBERS
bool CTiffProvider::OpenIOFiles(TIFF** pInfile, TIFF** pOutfile, bool bDeleteOutputFile)
{
	*pInfile = TIFFOpen(m_strInputFile.c_str(), "r");
	if (!*pInfile)
	{
		m_strErrorMsg = "Error opening input file: " + m_strInputFile;
		return false;
	}

	//create temp out file if no output file is given, if given, delete the old one and create new
	if (bDeleteOutputFile)
	{
		if (m_strOutputFile.empty())
		{
			//m_strOutputFile = m_strFilesPath + "tiff_temp.tif";
			m_bUseTempOutfile = true;
		}
		else
		{
			std::remove(m_strOutputFile.c_str());
		}
	}

	*pOutfile = TIFFOpen(m_strOutputFile.c_str(), "a");
	if (!*pOutfile)
	{
		m_strErrorMsg = "Error creating temporary file: " + m_strOutputFile;
		return false;
	}

	return true;
}

void CTiffProvider::CloseIOFiles(TIFF** pInfile, TIFF** pOutfile)
{
	TIFFClose(*pInfile);
	TIFFClose(*pOutfile);

	if (m_bUseTempOutfile)
	{
		std::remove(m_strInputFile.c_str());
		std::rename(m_strOutputFile.c_str(), m_strInputFile.c_str());
	}

	m_bUseTempOutfile = false;
}

uint16_t CTiffProvider::GetPageCount(TIFF* tif)
{
	//returns number images in a multipage TIFF file.
	uint16_t dircount = 0;

	do
	{
		dircount++;
	} while (TIFFReadDirectory(tif));

	return dircount;
}

TIFFParams& CTiffProvider::GetTIFFParams()
{
	return m_Params;
}

void CTiffProvider::SetTIFFParams(TIFFParams& tiffParams)
{
	m_Params = tiffParams;
}

void CTiffProvider::GetTagInfo(TIFF* pFile)
{
	TIFFGetField(pFile, TIFFTAG_IMAGEWIDTH, &m_TagHeader._width);
	TIFFGetField(pFile, TIFFTAG_IMAGELENGTH, &m_TagHeader._height);
	TIFFGetField(pFile, TIFFTAG_COMPRESSION, &m_TagHeader._compression);
	TIFFGetField(pFile, TIFFTAG_PLANARCONFIG, &m_TagHeader._config);
	TIFFGetField(pFile, TIFFTAG_PHOTOMETRIC, &m_TagHeader._photometric);
	TIFFGetField(pFile, TIFFTAG_ORIENTATION, &m_TagHeader._orientation);
	TIFFGetField(pFile, TIFFTAG_BITSPERSAMPLE, &m_TagHeader._bitspersample);
	TIFFGetField(pFile, TIFFTAG_SAMPLESPERPIXEL, &m_TagHeader._samplesperpixel);
}

int16_t CTiffProvider::WriteHeader(TIFF* tif, TagHeader& header)
{
	int res = 0;

	//some validations
	if ((header._orientation < ORIENTATION_TOPLEFT) || (header._orientation > ORIENTATION_LEFTBOT))
		header._orientation = ORIENTATION_TOPLEFT;

	res = TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, header._width);
	res = TIFFSetField(tif, TIFFTAG_IMAGELENGTH, header._height);
	res = TIFFSetField(tif, TIFFTAG_COMPRESSION, header._compression);
	res = TIFFSetField(tif, TIFFTAG_PLANARCONFIG, header._config);
	res = TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, header._photometric);
	res = TIFFSetField(tif, TIFFTAG_ORIENTATION, header._orientation);
	res = TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, header._bitspersample);
	res = TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, header._samplesperpixel);
	
	//JPEG compression dont support PALETTE type photometric and samplesperpixel == 1
	if ((header._photometric != PHOTOMETRIC_PALETTE) && (header._samplesperpixel > 1))
		header._compression = COMPRESSION_JPEG;
	else
		header._compression = m_CompressionTypes[m_Params._strCompressType];

	return (res == 1 ? sizeof(header) : -1);
}

bool CTiffProvider::WriteData(TIFF* pInfile, TIFF* pOutfile)
{
	bool bRes = true;
	
	if (!WriteHeader(pOutfile, m_TagHeader))
	{
		m_strErrorMsg = "Error writing the tag header info!!";
		return false;
	}

	//allocate memory for image by linesize (width * samplesperpixel)
	tmsize_t lineSize = TIFFScanlineSize(pInfile);
	unsigned char* pSourceImage = (unsigned char*)_TIFFmalloc(lineSize);
	
	//scan lines one by one in the current page and write to output page
	for (uint16_t row = 0; row < m_TagHeader._height; row++)
	{
		if (TIFFReadScanline(pInfile, pSourceImage, row) < 1)
		{
			_TIFFfree(pSourceImage);
			return false;
		}
		
		if (m_bToGrayScale || m_bToBinary)
		{
			if (m_bToGrayScale)
				ToGrayScale(pSourceImage, lineSize, m_TagHeader._samplesperpixel);
	
			if (m_bToBinary)
				ToGrayScale(pSourceImage, lineSize, m_TagHeader._samplesperpixel, true, m_Params._iThreshold);
	
			if (TIFFWriteScanline(pOutfile, pSourceImage, row, 0) < 0)
			{
				bRes = false;
				break;
			}
		}
		else
		{
			if (TIFFWriteScanline(pOutfile, pSourceImage, row, 0) < 0)
			{
				bRes = false;
				break;
			}
		}
	}

	TIFFFlush(pOutfile);
	_TIFFfree(pSourceImage);

	return bRes;
}

bool CTiffProvider::IsPageType(TIFF* pFile, m_ePageType pType)
{
	bool bResult = true;
	bool bColourPage = false;
	int iSamplesPerPixel = m_TagHeader._samplesperpixel;

	//usually white pixels have value 0xFF(255)
	uint16_t iWhitePixel = (m_TagHeader._photometric == PHOTOMETRIC_MINISWHITE) ? 0x00 : 0xFF;

	//For RGB/Grayscale with 3 samples per pixel.
	/*if (!ValidPixelFormat())
		return false;*/

	tmsize_t lineSize = TIFFScanlineSize(pFile);
	unsigned char* sourceImage = (unsigned char*)_TIFFmalloc(lineSize);

	//scan lines one by one in the current page 
	for (uint16_t row = 0; row < m_TagHeader._height; row++)
	{
		if (TIFFReadScanline(pFile, sourceImage, row) < 0)
		{
			_TIFFfree(sourceImage);
			return false;
		}

		//Check for page type (GRAYSCAL, COLOUR, BLANK)
		for (int index = 0; index < lineSize; index += iSamplesPerPixel)
		{
			if (pType == m_ePageType::BLANK)
			{
				if (sourceImage[index] != iWhitePixel)
				{
					bResult = false;
					break;
				}
			}
			else if ((pType == m_ePageType::COLOUR) || (pType == m_ePageType::GRAYSCALE))
			{
				//if samples per pixel == 3, RGB values should not be same
				unsigned char ch = sourceImage[index];
				unsigned char ch1 = sourceImage[index + 1];
				unsigned char ch2 = sourceImage[index + 2];
				if((ch | ch1 | ch2) != (ch & ch1 & ch2 ))
				{
					bColourPage = true;
					break;
				}
			}
			//else if (pType == m_ePageType::BINARY)
			//{
			//	if ((sourceImage[index] != 0xFF) && (sourceImage[index] != 0x00))
			//	{
			//		bResult = false;
			//		//break;
			//	}
			//}

			if (!bResult)
				break;
		}
	}

	_TIFFfree(sourceImage);

	//if not colour page, it is a grayscale
	if (pType == m_ePageType::GRAYSCALE)
		return !bColourPage;

	return bResult;
}

bool CTiffProvider::ValidPixelFormat()
{
	//PALETTE type photometric and one channel data are not supported for color converstions
	if ((m_TagHeader._photometric != PHOTOMETRIC_PALETTE) && (m_TagHeader._samplesperpixel == 3) || (m_TagHeader._samplesperpixel == 4))
		return true;

	return false;
}

bool CTiffProvider::ToGrayScale(unsigned char* pSourceImage, uint32_t lineSize, int iSamplesperpixel, bool bToBinary, int iThreshold)
{
	for (uint16_t index = 0; index < lineSize; index += iSamplesperpixel)
	{
		unsigned char R = pSourceImage[index];
		unsigned char G = pSourceImage[index + 1];
		unsigned char B = pSourceImage[index + 2];
		unsigned char grayPixel;

		//if R,G,B have sample values, this is grayscale pixel. So no need to change pixel value
		if ((R | G | B) != (R & G & B))
			grayPixel = (unsigned char)(0.2989 * R + 0.5870 * G + 0.1140 * B);
		else
			grayPixel = R;  // can also choose G or B values for graypixel

		if (m_bToGrayScale)
		{
			pSourceImage[index] = grayPixel;
			pSourceImage[index + 1] = grayPixel;
			pSourceImage[index + 2] = grayPixel;
			if (iSamplesperpixel == 4)
				pSourceImage[index + 3] = pSourceImage[index + 3]; //write Alpha channel AS IS.
		}
		else if (m_bToBinary)
		{
			unsigned char binaryPixel = (grayPixel > iThreshold) ? 0xFF : 0;
			pSourceImage[index] = binaryPixel;
			pSourceImage[index+1] = binaryPixel;
			pSourceImage[index+2] = binaryPixel;
			if (iSamplesperpixel == 4)
				pSourceImage[index + 3] = pSourceImage[index + 3]; //write Alpha channel AS IS.
		}
	}

	return true;
}

//PUBLIC MEMBERS
bool CTiffProvider::MergeFiles(std::string& infile1, std::string& infile2)
{
	bool bRes = true;

	m_strInputFile = infile2;
	m_strOutputFile = infile1;

	TIFF* pInfile1 = nullptr;
	TIFF* pInfile2 = nullptr;

	//we do "inplace merging here". we merge the two files by adding the contents of infile2 to infile1
	if (!OpenIOFiles(&pInfile2, &pInfile1, false))
		return false;

	//get the number of pages in the input TIFF file
	uint16_t iPageCount = GetPageCount(pInfile2);

	for (uint16_t pageno = 0; pageno < iPageCount; pageno++)
	{
		if (TIFFSetDirectory(pInfile2, pageno))
		{
			GetTagInfo(pInfile2);

			if (!WriteData(pInfile2, pInfile1))
			{
				m_strErrorMsg = "Error writing the data to destination!!";
				bRes = false;
				break;
			}
		}
	}

	CloseIOFiles(&pInfile1, &pInfile2);
	return bRes;
}

bool CTiffProvider::RemoveBlankPages(std::string& infile, std::string outfile)
{
	bool bRes = true;
	
	m_strInputFile = infile;
	m_strOutputFile = outfile;

	TIFF* pInfile = nullptr;
	TIFF* pOutfile = nullptr;

	if (!OpenIOFiles(&pInfile, &pOutfile))
		return false;

	uint16_t iPageCount = GetPageCount(pInfile);

	for (uint16_t pageno = 0; pageno < iPageCount; pageno++)
	{
		if (TIFFSetDirectory(pInfile, pageno))
		{
			//get the tagheader info from the input file
			GetTagInfo(pInfile);

			//Is the current page BLANK? If yes, dont process it.
			if (IsPageType(pInfile, m_ePageType::BLANK))
				continue;

			if (!WriteData(pInfile, pOutfile))
			{
				m_strErrorMsg = "Error writing the data to destination!!";
				bRes = false;
				break;
			}
		}
	}

	CloseIOFiles(&pInfile, &pOutfile);
	return bRes;
}

bool CTiffProvider::RemovePageByNumber(std::string& infile, std::set<uint16_t>& pNumbers, std::string outfile)
{
	bool bRes = true;

	m_strInputFile = infile;
	m_strOutputFile = outfile;

	TIFF* pInfile = nullptr;
	TIFF* pOutfile = nullptr;

	if (!OpenIOFiles(&pInfile, &pOutfile))
		return false;

	uint16_t iPageCount = GetPageCount(pInfile);
	
	for (uint16_t pno = 0; pno < iPageCount; pno++)
	{
		if (TIFFSetDirectory(pInfile, pno))
		{
			//remove the page if its page no. matches with the page to be removed
			if (pNumbers.find(pno + 1) != pNumbers.end())
				continue;

			//get the tagheader info from the input file
			GetTagInfo(pInfile);

			if (!WriteData(pInfile, pOutfile))
			{
				m_strErrorMsg = "Error: Failed to write the data to destination.";
				bRes = false;
				break;
			}
		}
	}

	CloseIOFiles(&pInfile, &pOutfile);
	return bRes;
}

//Miscellaneous operations
bool CTiffProvider::GetFileInfo(std::string& infile, std::string& fileinfo, std::string outfile)
{
	bool bRes = true;
	uint16_t iBlankpageCount = 0;
	uint16_t iTotalPages = 0;
	std::string strTagInfo = "";
	
	TIFF* pInfile = TIFFOpen(infile.c_str(), "r");
	if (!pInfile)
	{
		m_strErrorMsg = "Error opening input file: " + infile;
		return false;
	}
	
	iTotalPages = GetPageCount(pInfile);

	for (uint16_t pageno = 0; pageno < iTotalPages; pageno++)
	{
		if (TIFFSetDirectory(pInfile, pageno))
		{
			//get the tagheader info from the input file
			GetTagInfo(pInfile);

			//Is the current page BLANK? If yes, dont process it.
			if (IsPageType(pInfile, m_ePageType::BLANK))
				iBlankpageCount++;

			strTagInfo.append(("Page Number: " + std::to_string(pageno+1) += "\n"));
			strTagInfo.append("---------------\n");
			strTagInfo.append(("Width = " + std::to_string(m_TagHeader._width) + "\n"));
			strTagInfo.append(("Height = " + std::to_string(m_TagHeader._height) + "\n"));
			strTagInfo.append(("Layout(CONFIG) = " + std::to_string(m_TagHeader._config) + "\n"));
			strTagInfo.append(("Phtometric = " + std::to_string(m_TagHeader._photometric) + "\n"));
			strTagInfo.append(("Orientation = " + std::to_string(m_TagHeader._orientation) + "\n"));
			strTagInfo.append(("Compression = " + std::to_string(m_TagHeader._compression) + "\n"));
			strTagInfo.append(("Bits Per Sample = " + std::to_string(m_TagHeader._bitspersample) + "\n"));
			strTagInfo.append(("Samples Per Pixel = " + std::to_string(m_TagHeader._samplesperpixel) + "\n\n"));
		}
	}

	fileinfo.append(("Total number of pages: " + std::to_string(iTotalPages) + "\n"));
	fileinfo.append(("Total number of Blank pages: " + std::to_string(iBlankpageCount) + "\n\n"));
	fileinfo.append(strTagInfo);

	TIFFClose(pInfile);

	if (!outfile.empty())
	{
		FILE* pOutfile = nullptr;
		fopen_s(&pOutfile, outfile.c_str(), "w");
		if (!pOutfile)
		{
			m_strErrorMsg = "Error opening outfile: " + outfile;
			return false;
		}

		fprintf(pOutfile, "%s", fileinfo.c_str());
		fclose(pOutfile);
	}

	return true;
}

bool CTiffProvider::ConvertPageTo(std::string& infile, m_eConvertCode ccode, std::string outfile)
{
	bool bRes = true;
	bool bTempFile = false;

	m_strInputFile = infile;
	m_strOutputFile = outfile;

	TIFF* pInfile = nullptr;
	TIFF* pOutfile = nullptr;

	//open input/output files for processing
	if (!OpenIOFiles(&pInfile, &pOutfile))
		return false;

	uint16_t iPageCount = GetPageCount(pInfile);

	for (uint16_t pno = 0; pno < iPageCount; pno++)
	{
		if (TIFFSetDirectory(pInfile, pno))
		{
			//get the tagheader info from the input file
			GetTagInfo(pInfile);

			//if the input file is not a BLANK page, and has valid pixel format, then proceed for conversion
			if (!IsPageType(pInfile, m_ePageType::BLANK) && ValidPixelFormat())
			{
				m_bToGrayScale = (ccode == m_eConvertCode::TOGRAY) ? true : false;
				m_bToBinary = (ccode == m_eConvertCode::TOBINARY) ? true : false;
			}
			
			if (!WriteData(pInfile, pOutfile))
			{
				m_strErrorMsg = "Error: Failed to write the data to destination.";
				bRes = false;
				break;
			}

			m_bToGrayScale = false;
			m_bToBinary = false;
			TIFFFlush(pOutfile);
		}
	}

	CloseIOFiles(&pInfile, &pOutfile);
	return bRes;
}

std::string CTiffProvider::GetErrorMsg()
{
	return m_strErrorMsg;
}