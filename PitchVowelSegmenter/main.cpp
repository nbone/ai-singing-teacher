/* Nicholas Bone, February 2010, Master's research at WWU */
/* Substantially modified February 2014 */

/* This console utility takes a sound file and extracts a sequence of single
 * pitch single vowel chunks, along with associated text files containing
 * time-stamped feature vectors for training and classification.
 * Requires:
 *   - 
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>


// These files must be in the current directory or in the PATH
#define PRAAT_EXE			"praatcon.exe"
#define FFMPEG_EXE			"ffmpeg.exe -v 0"
    // "-v 0" = force quiet mode
#define PRAAT_SCRIPT_FEAT	"extractFeatures.praat"
#define PRAAT_SCRIPT_SLICE	"extractSoundSlice.praat"

// Default values for command-line options
#define DEFAULT_TIME_STEP	0.02
#define DEFAULT_WINDOW      0.25
#define DEFAULT_MAX_FORMANT	5500
#define THRESHOLD_PITCH		12
#define THRESHOLD_F1		15
#define THRESHOLD_F2		20

#define szFILE_EXT_DATA		".txt"
#define MATRIX_HEADER		"Time,Intensity,Pitch,F1,F2,F3,MFCC1,MFCC2,MFCC3,MFCC4,MFCC5,MFCC6,MFCC7,MFCC8,MFCC9,MFCC10,MFCC11,MFCC12\n"
#define FIELD_UNDEFINED		"--undefined--"
#define MAX_FIELD			15
#define NUM_FIELDS			18
#define MAX_LINE			300
#define DELIM				','
#define cNULL				'\0'

#define MINIMUM_INTENSITY	55

#define EXTRACT_MS	500

using namespace std;

struct FeatureVector {
	long Time;		// in milliseconds
	int Intensity;	// in decibles
	int Pitch;		// in Hertz
	int F1;			// in Hertz
	int F2;			// in Hertz
	int F3;			// in Hertz
	int MFCC1;		// in ?
	int MFCC2;		// in ?
	int MFCC3;		// in ?
	int MFCC4;		// in ?
	int MFCC5;		// in ?
	int MFCC6;		// in ?
	int MFCC7;		// in ?
	int MFCC8;		// in ?
	int MFCC9;		// in ?
	int MFCC10;		// in ?
	int MFCC11;		// in ?
	int MFCC12;		// in ?
};

struct Options {
	int Verbosity;		// 0 = quiet, etc...
	bool IsTestMode;
	char szInputFilePath[MAX_PATH];	// the sound file (presumably .wav or .mp3)
	char szBaseFilePath[MAX_PATH];	// just path/name, with extension trimmed
    double TimeStep;	// seconds
    double Window;		// seconds, formant analysis window
    int MaxFormant;		// Hertz; use 5500 for female and 5000 for male voices
    double PitchThreshold;	// percent change
    double F1Threshold;	// percent change
    double F2Threshold;	// percent change
};

// forward declarations:
void extractSegment(long tStart, long tEnd, FILE *& fpOutData, char szInputFilePath[], char szBaseFilePath[], char szWorkingFilePath[]);
bool isSegmentSuitableForExtract(const struct Options options, const struct FeatureVector features[], const int nBufferPoints);
bool isVoiced(const struct FeatureVector& fv);
bool parseLine(struct FeatureVector& fv, char * const szLine);
bool getField(char * pDest, int size, char ** ppSource, const char cDelim);
double calculatePercentDifference(double a, double b);

// USAGE: PitchVowelSegmenter.exe path_to_some_sound.wav
// Creates zero or more path_to_some_sound_t1_t2.[wav,txt] files, where
// t1 and t2 are the start/stop times (in milliseconds) of that chunk,
// and the .txt is the feature matrix associated with that .wav.

void displayUsage() {
    cout << "USAGE: PitchVowelSegmenter.exe <soundFile> [options]" << endl
    	<< "Options:" << endl
    	<< "    -t timeStep     : analysis time step, in seconds (default: " << DEFAULT_TIME_STEP << ")" << endl
    	<< "    -w windowWidth  : formant analysis window, in seconds (default: " << DEFAULT_WINDOW << ")" << endl
    	<< "    -mf maxFormant  : maximum formant, in Hz (default: " << DEFAULT_MAX_FORMANT << ")" << endl
    	<< "            recommended values are 5500 for female and 5000 for male voices" << endl
    	<< "    -dp pitchPercent: maximum allowed % variance in Pitch within a segment (default: " << THRESHOLD_PITCH << ")" << endl
    	<< "    -df1 f1Percent  : maximum allowed % variance in F1 within a segment (default: " << THRESHOLD_F1 << ")" << endl
    	<< "    -df2 f2Percent  : maximum allowed % variance in F2 within a segment (default: " << THRESHOLD_F2 << ")" << endl
    	<< "    -v verbosity    : 0 = normal; 1 = basic diagnostic; 2 = line-by-line output" << endl
    	<< "    -test           : run analysis but skip file extraction" << endl
    	<< endl;
}

void ensurePositive(double value, const char name[]) {
	if (value <= 0) {
		cout << "ERROR: " << name << " must be positive" << endl;
		displayUsage();
		exit(1);
	}
}

Options parseCommandLine(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "ERROR: missing required argument" << endl;
        displayUsage();
        exit(1);
    }
    
    Options options;

	// Parse required first argument (sound file path)
	strcpy(options.szInputFilePath, argv[1]);
	strcpy(options.szBaseFilePath, argv[1]);
	int MAX_EXT = 8;//sizeof(options.szFileExtSound) - 1;
	for (int i = strlen(options.szInputFilePath) - 1; i > strlen(options.szInputFilePath) - MAX_EXT; i--) {
		if ('.' == options.szInputFilePath[i-1]) {
			options.szBaseFilePath[i-1] = cNULL;
			break;
		}
	}

	// Set defaults for other options
	options.Verbosity = 0;
	options.IsTestMode = false;
	options.TimeStep = DEFAULT_TIME_STEP;
	options.Window = DEFAULT_WINDOW;
	options.MaxFormant = DEFAULT_MAX_FORMANT;
	options.PitchThreshold = THRESHOLD_PITCH;
	options.F1Threshold = THRESHOLD_F1;
	options.F2Threshold = THRESHOLD_F2;
	
	// Parse optional arguments
	try {
		for (int n = 2; n < argc; n++) {
			if (!strcmp("-t", argv[n])) {
				n++;
				options.TimeStep = atof(argv[n]);
				ensurePositive(options.TimeStep, "timeStep");
				continue;
			}
			if (!strcmp("-w", argv[n])) {
				n++;
				options.Window = atof(argv[n]);
				ensurePositive(options.Window, "windowWidth");
				continue;
			}
			if (!strcmp("-mf", argv[n])) {
				n++;
				options.MaxFormant = atoi(argv[n]);
				ensurePositive(options.MaxFormant, "maxFormant");
				continue;
			}
			if (!strcmp("-dp", argv[n])) {
				n++;
				options.PitchThreshold = atof(argv[n]);
				ensurePositive(options.PitchThreshold, "pitchPercent");
				continue;
			}
			if (!strcmp("-df1", argv[n])) {
				n++;
				options.F1Threshold = atof(argv[n]);
				ensurePositive(options.F1Threshold, "f1Percent");
				continue;
			}
			if (!strcmp("-df2", argv[n])) {
				n++;
				options.F2Threshold = atof(argv[n]);
				ensurePositive(options.F2Threshold, "f2Percent");
				continue;
			}
			if (!strcmp("-v", argv[n])) {
				n++;
				options.Verbosity = atoi(argv[n]);
				continue;
			}
			if (!strcmp("-test", argv[n])) {
				options.IsTestMode = true;
				continue;
			}
			cout << "ERROR: unrecognized argument '" << argv[n] << "'" << endl;
			displayUsage();
			exit(1);
		}
	} catch (...) {
		cout << "ERROR: invalid command line" << endl;
		displayUsage();
		exit(1);
	}
	
	return options;
}

int main(int argc, char *argv[]) {
	char szFeaturesFilePath[MAX_PATH];	// the generated features for the whole sound
	char szWorkingFilePath[MAX_PATH];	// "working" file for chunks-in-progress
	char szCommandBuffer[MAX_PATH];
	
	Options options = parseCommandLine(argc, argv);
	
	strcpy(szFeaturesFilePath, options.szBaseFilePath);
	strcat(szFeaturesFilePath, szFILE_EXT_DATA);
	strcpy(szWorkingFilePath, options.szBaseFilePath);
	strcat(szWorkingFilePath, "_TEMP");
	strcat(szWorkingFilePath, szFILE_EXT_DATA);

	// Call praat script to generate feature matrix from input file
	sprintf(szCommandBuffer, "%s %s %s %s %f %f %d",
		PRAAT_EXE, PRAAT_SCRIPT_FEAT, options.szInputFilePath, szFeaturesFilePath, options.TimeStep, options.Window, options.MaxFormant );
	system(szCommandBuffer);

	// Open feature matrix file created by praat script
	FILE * fpMatrix = fopen(szFeaturesFilePath, "r");
	if (fpMatrix == NULL) {
		cout << "couldn't open file " << szFeaturesFilePath << " for reading" << endl;
		exit(1);
	}
	// Check that file header matches expected format:
	char szHeader[MAX_LINE];
	fgets(szHeader, MAX_LINE, fpMatrix);
	if (strcmp(szHeader, MATRIX_HEADER)) {
		cout << "file header doesn't match" << endl;
		exit(1);
	}
	
	// Init output file for "chunk" of feature data
	FILE * fpOutData = fopen(szWorkingFilePath, "w");
	if (fpOutData == NULL) {
		cout << "couldn't open file " << szWorkingFilePath << " for writing" << endl;
		exit(1);
	}
	if (0 > fputs(MATRIX_HEADER, fpOutData)) {
		cout << "couldn't write to file " << szWorkingFilePath << endl;
		exit(1);
	}
	
	int nBufferPoints = (int)(0.5 + (EXTRACT_MS / 1000.0) / options.TimeStep);

	// Diagnostic output
	if (options.Verbosity > 0) {
		cout << "Analysis time step: " << options.TimeStep << endl
			<< "Points per segment: " << nBufferPoints << " (" << nBufferPoints * options.TimeStep << " seconds)" << endl;
	}
	
	#define MAX_BUFFER_SIZE 100
	if (nBufferPoints > MAX_BUFFER_SIZE) {
		cout << "ERROR: MAX_BUFFER_SIZE is set to " << MAX_BUFFER_SIZE << " which is not sufficient to store a segment." << endl;
		exit(1);
	}
	
	// Read feature matrix line-by-line; look for boundary points
	char szLineBuffer[MAX_BUFFER_SIZE][MAX_LINE];
	struct FeatureVector features[MAX_BUFFER_SIZE];
	int countPointsInCurrentSegment = 0;
	int countExtractedSegments = 0;
	double totalExtractedSegmentTime = 0.0;
	bool isUnvoiced = false;
	int line = 0;
	for (line = 0; !feof(fpMatrix) && !ferror(fpMatrix) && !ferror(fpOutData); line++) {
		fgets(szLineBuffer[line % nBufferPoints], MAX_LINE, fpMatrix);
		if (!parseLine(features[line % nBufferPoints], szLineBuffer[line % nBufferPoints])) {
			cout << "line didn't parse" << endl;
			exit(1);
		}
		countPointsInCurrentSegment++;
		
		// If current point is unvoiced then discard current segment and start anew
		isUnvoiced = !isVoiced(features[line % nBufferPoints]);
		if (isUnvoiced) {
			countPointsInCurrentSegment = 0;
		}
		
		// Diagnostic output
		if (options.Verbosity > 1) {
			cout << "LINE " << line << " : (t)"
				<< features[line % nBufferPoints].Time << ", (p)" << features[line % nBufferPoints].Pitch
				<< ", (f1)" << features[line % nBufferPoints].F1 << ", (f2)" << features[line % nBufferPoints].F2
				<< " : (" << (isUnvoiced ? "UNVOICED" : "voiced") << ")" << endl;
		}
		
		// If we have sufficient points in this segment and it's sufficiently homogenous then extract it
		if (countPointsInCurrentSegment == nBufferPoints) {
			if (isSegmentSuitableForExtract(options, features, nBufferPoints)) {
				int nStartPoint = (line + 1 ) % nBufferPoints;
				int nEndPoint = line % nBufferPoints;
				int tStart = features[nStartPoint].Time;
				int tEnd = features[nEndPoint].Time;
				
				if (!options.IsTestMode) {
					// Dump buffer into output file
					for (int k = 0; k < nBufferPoints; k++) {
						fputs(szLineBuffer[(nStartPoint + k) % nBufferPoints], fpOutData);
					}
					
					// Extract segment
					extractSegment(tStart, tEnd, fpOutData, options.szInputFilePath, options.szBaseFilePath, szWorkingFilePath);
				}
				
				// Reset count to start a new segment
				countPointsInCurrentSegment = 0;
				
				// diagnostic output
				std::ios_base::fmtflags oldFmtflags = cout.setf(std::ios::fixed);
				int oldPrecision = cout.precision(3);
				
				double tStartSeconds = tStart / 1000.0;
				double tEndSeconds = tEnd / 1000.0;
				totalExtractedSegmentTime += tEndSeconds - tStartSeconds;
				countExtractedSegments++;
				
				cout << "EXTRACT:" << std::setw(7) << tStartSeconds << " to" << std::setw(7) << tEndSeconds
					<< " (" << tEndSeconds - tStartSeconds << "s)" << endl;
				
				cout.setf(oldFmtflags);
				cout.precision(oldPrecision);
			} else {
				// Segment didn't pass test for extract; keep sliding the window along one point at a time
				countPointsInCurrentSegment--;
			}
		}
	}

	// don't delete the source files; leave to caller to decide how best to clean up

	cout << "done processing " << line << " lines" << endl;
	if (countExtractedSegments > 0) {
		cout << "extracted " << countExtractedSegments << " segments with mean duration " << setprecision(3) << totalExtractedSegmentTime / countExtractedSegments << " seconds" << endl;
	} else {
		cout << "extracted NO segments" << endl;
	}

    return EXIT_SUCCESS;
}

void extractSegment(long tStart, long tEnd, FILE *& fpOutData, char szInputFilePath[], char szBaseFilePath[], char szWorkingFilePath[]) {
	char szOutputFilePath[MAX_PATH];
	char szCommandBuffer[MAX_PATH];

	// Extract feature chunk:
	sprintf(szOutputFilePath, "%s_%ld_%ld%s",
		szBaseFilePath, tStart, tEnd, szFILE_EXT_DATA );
	fclose(fpOutData);
	if (0 != rename(szWorkingFilePath, szOutputFilePath)) {
		cout << "failed to rename " << szWorkingFilePath << " to " << szOutputFilePath << endl;
		exit(1);
	}
	fpOutData = fopen(szWorkingFilePath, "w");
	fputs(MATRIX_HEADER, fpOutData);
	if (NULL == fpOutData) {
		cout << "couldn't re-open file " << szWorkingFilePath << " for writing" << endl;
		exit(1);
	}
	
	// Extract sound chunk (via praat script--NOTE that it only outputs WAV files, regardless of source encoding):
	sprintf(szOutputFilePath, "%s_%ld_%ld",
		szBaseFilePath, tStart, tEnd );
	sprintf(szCommandBuffer, "%s %s %s %s.wav %f %f",
		PRAAT_EXE, PRAAT_SCRIPT_SLICE, szInputFilePath, szOutputFilePath, tStart/1000.0, tEnd/1000.0 );
	system(szCommandBuffer);
	
	// Call ffmpeg to convert WAV to MP3 to save space:
	sprintf(szCommandBuffer, "%s -i %s.wav %s.mp3",
		FFMPEG_EXE, szOutputFilePath, szOutputFilePath );
	system(szCommandBuffer);
	
	// Remove WAV file:
	strcat(szOutputFilePath, ".wav");
	remove(szOutputFilePath);
}

double calculatePercentDifference(double a, double b) {
	if (a == b)
		return 0;
	if (a == 0 || b == 0)
		return 1000; // doesn't matter as long as it's bigger than any threshold
	double quotient = a > b
		? a / b
		: b / a;
	return 100 * (quotient - 1);
}

bool isSegmentSuitableForExtract(const struct Options options, const struct FeatureVector features[], const int nBufferPoints) {
	// Calculate min/max range of Pitch, F1, and F2, and ensure it's within the tolerance
	int minPitch = 0xFFFF;
	int maxPitch = 0;
	int minF1 = 0xFFFF;
	int maxF1 = 0;
	int minF2 = 0xFFFF;
	int maxF2 = 0;
	
	int minTime = 0xFFFF;
	
	char szP12[4];
	memset(szP12, ' ', sizeof(szP12));
	szP12[sizeof(szP12)-1] = cNULL;
	
	bool ret = true;
	
	for (int i = 0; i < nBufferPoints; i++) {
		if (features[i].Pitch < minPitch)
			minPitch = features[i].Pitch;
		if (features[i].Pitch > maxPitch)
			maxPitch = features[i].Pitch;
		if (features[i].F1 < minF1)
			minF1 = features[i].F1;
		if (features[i].F1 > maxF1)
			maxF1 = features[i].F1;
		if (features[i].F2 < minF2)
			minF2 = features[i].F2;
		if (features[i].F2 > maxF2)
			maxF2 = features[i].F2;
		if (features[i].Time < minTime)
			minTime = features[i].Time;
	}

	double pitchDifference = calculatePercentDifference(minPitch, maxPitch);
	double f1Difference = calculatePercentDifference(minF1, maxF1);
	double f2Difference = calculatePercentDifference(minF2, maxF2);
	
	if (pitchDifference > options.PitchThreshold) {
		szP12[0] = 'P';
		ret = false;
	}
	if (f1Difference > options.F1Threshold) {
		szP12[1] = '1';
		ret = false;
	}
	if (f2Difference > options.F2Threshold) {
		szP12[2] = '2';
		ret = false;
	}

	std::ios_base::fmtflags oldFmtflags = cout.setf(std::ios::fixed);
	
	if (options.Verbosity > 1 || (options.Verbosity > 0 && ret)) {
		cout << setw(9) << minTime << ": [" << szP12 << "]"
			<< " P:[" << minPitch << "," << maxPitch << "](" << setprecision(1) << pitchDifference << ")"
			<< " F1:[" << minF1 << "," << maxF1 << "](" << setprecision(1) << f1Difference << ")"
			<< " F2:[" << minF2 << "," << maxF2 << "](" << setprecision(1) << f2Difference << ")"
			<< endl;
	}
	
	cout.setf(oldFmtflags);

	return ret;
}

bool isVoiced(const struct FeatureVector& fv) {
	if (fv.Intensity < MINIMUM_INTENSITY)
		return false;
	if (fv.Pitch == 0)
		return false;
	if (fv.F3 == 0)
		return false;
	return true;
}

bool getField(char * pDest, int size, char ** ppSource, const char cDelim) {
	// copy from source to dest until: (a) size, (b) null, or (c) delim
	// ALSO advance source pointer!
	// return true unless first char of source is null
	// null-terminate dest
	if (cNULL == **ppSource) {
		return false;	// end of source!
	}
	int count = 0;
	for (count = 0; count < size; count++) {
		pDest[count] = (*ppSource)[count];
		if ((cDelim == pDest[count]) || (cNULL == pDest[count]) || ('\n' == pDest[count])) {
			pDest[count] = cNULL;
			break;	// found end of field, so stop loop
		}
	}
	// Update source pointer:
	*ppSource += count;
	if (cDelim == **ppSource) {
		++*ppSource;	// skip this delimiter
	} else if ((cNULL != **ppSource) && (size == count)) {
		return false;	// buffer size was too small for actual field (data may be corrupt)
	}
	// Truncate dest field just in case:
	pDest[size-1] = cNULL;
	// Convert "undefined" fields into zero value:
	if (!strcmp(pDest, FIELD_UNDEFINED)) {
		pDest[0] = '0';
		pDest[1] = cNULL;
	}
	return true;
}

bool parseLine(struct FeatureVector& fv, char * const szLine) {
	// Checks that line has enough fields that aren't too wide, but doesn't check for bad data (e.g. letters instead of numbers)
	char szField[MAX_FIELD+1];	// +1 for NULL
	char * pcLine = szLine;
	// Time (convert seconds to milliseconds)
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.Time = (long)(1000 * atof(szField));
	// Intensity
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.Intensity = atoi(szField);
	// Pitch
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.Pitch = atoi(szField);
	// F1
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.F1 = atoi(szField);
	// F2
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.F2 = atoi(szField);
	// F3
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.F3 = atoi(szField);
return true;
	// MFCC1
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC1 = atoi(szField);
	// MFCC2
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC2 = atoi(szField);
	// MFCC3
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC3 = atoi(szField);
	// MFCC4
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC4 = atoi(szField);
	// MFCC5
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC5 = atoi(szField);
	// MFCC6
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC6 = atoi(szField);
	// MFCC7
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC7 = atoi(szField);
	// MFCC8
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC8 = atoi(szField);
	// MFCC9
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC9 = atoi(szField);
	// MFCC10
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC10 = atoi(szField);
	// MFCC11
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC11 = atoi(szField);
	// MFCC12
	if (!getField(szField, sizeof(szField), &pcLine, DELIM)) return false;
	fv.MFCC12 = atoi(szField);
	return true;
}
