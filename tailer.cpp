#include <iostream>
#include <fstream>
#include <time.h>
#include <vector>
#include <beanstalk.hpp>


using namespace std;
using namespace Beanstalk;

ifstream::pos_type size;
char * memblock;

char alertLocator[] = "** Alert";


int startingPos(char * memblock, int currentPosition)
{
	int locatorLength = strlen(alertLocator);

	for(int i = 0; i<locatorLength; i++)
	{
		// Create an internal position which is the position to search within the memblock.
		int internalPosition = currentPosition + i;

		// Get the current char so we can perform a comparison
		char currentChar = memblock[internalPosition];

		if(currentChar != alertLocator[i])
		{
			return -1;
		}
	}

	return currentPosition;
}


/**
 * Main handler holds loop open to get fsize.
 */
int main ()
{

	Client beanstalkClient("127.0.0.1", 11300);

	beanstalkClient.use("raw-alerts");

	ifstream file ("/var/ossec/logs/alerts/alerts.log", ios::in|ios::ate);

	if (!file.is_open())
	{
		cout << "Unable to open file" << endl;
		return 1;
	}

	int startingSize = file.tellg();

	cout << "Starting size: " << startingSize << " bytes" << endl;

	while(true)
	{
		sleep(1);

		// Go to the end of the file again and get the new position.

		file.seekg(0, ios::end);

		int currentSize = file.tellg();
		if(currentSize == startingSize)
		{
			continue;
		}

		// If the current size is less than the starting size, the file has been truncated so start again!
		if(currentSize < startingSize)
		{
			cout << "File must have been truncated. Starting from 0 again in total size..";
			startingSize = 0;
		}


		/**
		 * The size of the file must have grown
		 * so we want to seek back to our last known point
		 * and then seek up to this point.
		 */
		file.seekg(startingSize, ios::beg);
		memblock = new char [currentSize - startingSize];
		file.read (memblock, currentSize - startingSize);

		// Writing the starting size with the now new size.
		startingSize = currentSize;

		int memSize = strlen(memblock);

		vector<int> alertPositions;

		cout << memSize << "bytes have been written into memory. I am going to process that now.." << endl;

		for(int i = 0; i<= memSize; i++)
		{

			char currentChar = memblock[i];

			if(currentChar != '*')
			{
				continue;
			}

			int alertStartingPos = startingPos(memblock, i);


			if(alertStartingPos < 0)
			{
				continue;
			}

			i += strlen(alertLocator);

			alertPositions.push_back(alertStartingPos);

		}

		int endingIterator = alertPositions.size();
		int jobsWritten = 0;
		int lastAlertOccurance = -1;

		for (int iterator = 0; iterator < endingIterator; ++iterator)
		{
			if(-1 == lastAlertOccurance)
			{
				lastAlertOccurance = alertPositions[iterator];
				continue;
			}

			int currentOccurance = alertPositions[iterator];

			char alertBlock [(currentOccurance - lastAlertOccurance)];

			int actualBlockIterator = 0;
			for(int internalBlockIterator = lastAlertOccurance; internalBlockIterator < currentOccurance; internalBlockIterator++)
			{
				alertBlock[actualBlockIterator] = memblock[internalBlockIterator];
				++actualBlockIterator;
			}


			beanstalkClient.put(alertBlock, 0, 0, 0);
			++jobsWritten;
		}

		cout << "Wrote " << jobsWritten << " jobs to beanstalkd.." << endl;
	}

  return 0;
}

