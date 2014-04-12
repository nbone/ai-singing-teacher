# Extracts time, intensity, pitch, F1, F2, F3, MFCC1, ..., MFCC12 from the selected sound
# and writes to a CSV text file.
# NOTE: this is designed to be called from another script or program (but can be used manually).

# inputs:
# - soundFile$ = path to the sound file to open
# - outputFile$ = path to the file to write the features to
# - tstep = time step for feature calculation
# - window = width of analysis window for formant calculation
# - maxFormant = maximum formant (Hz) in formant calculation; use 5500 for female and 5000 for male voices

# Initialize parameters for the extraction:
form Specify input sound, output file, time step, and analysis window
  sentence soundFile
  sentence outputFile
  positive tstep 0.02
  positive window 0.25
  positive maxFormant 5500
endform

# Extract Right channel from stereo sound to get mono signal (slightly smoother formant curves?)
Read from file... 'soundFile$'
Extract one channel... Right
mySound = selected ("Sound")

# Get the pitch (1500 Hz > F#6, should be sufficient for very high soprano):
To Pitch (ac)... 'tstep' 75 15 no 0.01 0.15 0.05 0.35 0.14 1500
myPitch = selected ("Pitch")

# Get the intensity:
select mySound
To Intensity... 100 'tstep' yes
myIntensity = selected ("Intensity")

# Get the formants:
select mySound
To Formant (burg)... 'tstep' 5 'maxFormant' 'window' 50
myFormant = selected ("Formant")
numRows = Get number of frames
tstart = Get time from frame number... 1

# Convert formant data to matrix for fast iteration:
Down to Table... no no 1 no 1 no 1 no
myTable = selected ("Table")
Down to Matrix
myFormantMatrix = selected ("Matrix")

# Get the MFCCs, and convert to matrix same as formants:
select mySound
To MFCC... 12 'window' 'tstep' 100 100 0
myMFCC = selected ("MFCC")
To Matrix
myMFCCMatrixTemp = selected ("Matrix")
Transpose
myMFCCMatrix = selected ("Matrix")

# Erase output file, and write header row at top:
filedelete 'outputFile$'
fileappend "'outputFile$'" Time,Intensity,Pitch,F1,F2,F3,MFCC1,MFCC2,MFCC3,MFCC4,MFCC5,MFCC6,MFCC7,MFCC8,MFCC9,MFCC10,MFCC11,MFCC12'newline$'

# Iterate over formant matrix, write one row per line to out file:
for i from 1 to numRows
    # Get the formants:
    select myFormantMatrix
    f1 = Get value in cell... i 1
    f2 = Get value in cell... i 2
    f3 = Get value in cell... i 3
    # Calculate the time from the row number:
    time = tstart + (i-1) * tstep
    # Get nearest intensity and pitch for this time:
    select myIntensity
    intensity = Get value at time... 'time' Nearest
    select myPitch
    pitch = Get value at time... 'time' Hertz Nearest
    # Get the MFCCs:
    select myMFCCMatrix
    c1 = Get value in cell... i 1
    c2 = Get value in cell... i 2
    c3 = Get value in cell... i 3
    c4 = Get value in cell... i 4
    c5 = Get value in cell... i 5
    c6 = Get value in cell... i 6
    c7 = Get value in cell... i 7
    c8 = Get value in cell... i 8
    c9 = Get value in cell... i 9
    c10 = Get value in cell... i 10
    c11 = Get value in cell... i 11
    c12 = Get value in cell... i 12
    # Write to file:
    fileappend "'outputFile$'" 'time:3','intensity:0','pitch:0','f1:0','f2:0','f3:0','c1:0','c2:0','c3:0','c4:0','c5:0','c6:0','c7:0','c8:0','c9:0','c10:0','c11:0','c12:0''newline$'
endfor

# Clean up:
select myPitch
plus myIntensity
plus myFormant
plus myTable
plus myFormantMatrix
plus myMFCC
plus myMFCCMatrixTemp
plus myMFCCMatrix
Remove
select mySound
