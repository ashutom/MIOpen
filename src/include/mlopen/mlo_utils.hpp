/**********************************************************************
Copyright (c)2016 Advanced Micro Devices, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************/


#ifndef MLO_UITLS_H_
#define MLO_UITLS_H_

typedef std::pair<size_t, void*> mlo_ocl_arg;
typedef std::map<int, mlo_ocl_arg> mlo_ocl_args;

#ifdef MLOPEN


#ifdef WIN32

 inline double
	 mlopen_mach_absolute_time(void)   // Windows
 {
	 double ret = 0;
	 __int64 frec;
	 __int64 clocks;
	 QueryPerformanceFrequency((LARGE_INTEGER *)&frec);
	 QueryPerformanceCounter((LARGE_INTEGER *)&clocks);
	 ret = (double)clocks * 1000. / (double)frec;
	 return(ret);
 }
#else
 // We want milliseconds. Following code was interpreted from timer.cpp
 inline double
	 mlopen_mach_absolute_time()   // Linux 
 {
	 double  d = 0.0;
	 timeval t{}; t.tv_sec = 0; t.tv_usec = 0;
	 gettimeofday(&t, nullptr);
	 d = (t.tv_sec*1000.0) + t.tv_usec / 1000;  // TT: was 1000000.0 
	 return(d);
 }
#endif


 inline double
	 subtractTimes(double endTime, double startTime)
 {
	 double difference = endTime - startTime;
	 static double conversion = 0.0;

	 if (conversion == 0.0)
	 {
#ifdef __APPLE__
		 mach_timebase_info_data_t info{};
		 kern_return_t err = mach_timebase_info(&info);

		 //Convert the timebase into seconds
		 if (err == 0) {
			 conversion = 1e-9 * static_cast<double>(info.numer) / static_cast<double>(info.denom);
}
#else
		 conversion = 1.;
#endif
	 }
	 return conversion * difference;
 }

#endif



	 /**
	 * for the opencl program file processing
	 */
class mloFile {
public:
	/**
	*Default constructor
	*/
	mloFile() : source_("") {}

	/**
	* Destructor
	*/
	~mloFile()= default;;

	/**
	* Opens the CL program file
	* @return true if success else false
	*/
	bool open(const char *fileName) {
		size_t size;
		char *str;
		// Open file stream
		std::fstream f(fileName, (std::fstream::in | std::fstream::binary));
		// Check if we have opened file stream
		if (f.is_open()) {
			size_t sizeFile;
			// Find the stream size
			f.seekg(0, std::fstream::end);
			size = sizeFile = (size_t)f.tellg();
			f.seekg(0, std::fstream::beg);
			str = new char[size + 1];
			if (!str) {
				f.close();
				return false;
			}
			// Read file
			f.read(str, sizeFile);
			f.close();
			str[size] = '\0';
			source_ = str;
			delete[] str;
			return true;
		}
		return false;
	}

	/**
	* writeBinaryToFile
	* @param fileName Name of the file
	* @param binary char binary array
	* @param numBytes number of bytes
	* @return true if success else false
	*/
	int writeBinaryToFile(const char *fileName, const char *binary,
		size_t numBytes) {
		FILE *output = nullptr;

		if (fopen_s(&output, fileName, "wb")) {
			return 0;
		}
		fwrite(binary, sizeof(char), numBytes, output);
		fclose(output);
		return 0;
	}

	/**
	* readBinaryToFile
	* @param fileName name of file
	* @return true if success else false
	*/
	int readBinaryFromFile(const char *fileName) {
		FILE *input = nullptr;
		size_t size = 0, val;
		char *binary = nullptr;

		if (fopen_s(&input, fileName, "rb")) {
			return -1;
		}
		fseek(input, 0L, SEEK_END);
		size = ftell(input);
		rewind(input);
		binary = reinterpret_cast<char *>(malloc(size));
		if (binary == nullptr) {
			return -1;
		}
		val = fread(binary, sizeof(char), size, input);
		fclose(input);
		source_.assign(binary, size);
		free(binary);
		return 0;
	}

	/**
	* Replaces Newline with spaces
	*/
	void replaceNewlineWithSpaces() {
		size_t pos = source_.find_first_of('\n', 0);
		while (pos != -1) {
			source_.replace(pos, 1, " ");
			pos = source_.find_first_of('\n', pos + 1);
		}
		pos = source_.find_first_of('\r', 0);
		while (pos != -1) {
			source_.replace(pos, 1, " ");
			pos = source_.find_first_of('\r', pos + 1);
		}
	}

	/**
	* source
	* Returns a pointer to the string object with the source code
	*/
	const std::string &source() const { return source_; }

private:
	/**
	* Disable copy constructor
	*/
	mloFile(const mloFile &);

	/**
	* Disable operator=
	*/
	mloFile &operator=(const mloFile &);

	std::string source_; //!< source code of the CL program

};

 
inline void tokenize(const std::string& str,
	std::vector<std::string>& tokens,
	const std::string& delimiters = " ");

inline void tokenize(const std::string& str,
	std::vector<std::string>& tokens,
	const std::string& delimiters)
{
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos || std::string::npos != lastPos)
	{
		// Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
}
#endif
