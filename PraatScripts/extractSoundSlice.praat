# Extracts a specified time range from the selected sound and writes to a new file.
# NOTE: this is designed to be called from another script or program (but can be used manually).

# inputs:
# - soundFile$ = path to the sound file to open
# - outputFile$ = path to the file to write the sound part to
# - tstart = time at which to begin the slice (in seconds)
# - tend = time at which to end the slice (in seconds)

# Initialize parameters for the extraction:
form Specify input sound, output file, and time range
  sentence soundFile
  sentence outputFile
  positive tstart
  positive tend
endform

Read from file... 'soundFile$'
mySound = selected ("Sound")

Extract part... tstart tend rectangular 1 yes
mySoundPart = selected ("Sound")

Write to WAV file... 'outputFile$'

select mySound
