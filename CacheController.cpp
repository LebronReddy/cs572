/*
	Cache Simulator Implementation by Justin Goins
	Oregon State University
	Fall Term 2018
*/

#include "CacheController.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <cmath>

using namespace std;

CacheController::CacheController(ConfigInfo ci, string tracefile, mutex *Accessbus, mutex *Localbus, vector<list<unsigned long>> invalidation_queue, mutex *locker, atomic<int> *thread_counter, condition_variable *cond) {
	// store the configuration info
	this->ci = ci;
	this->inputFile = string(tracefile);
	this->outputFile = this->inputFile + ".out";
	// compute the other cache parameters
	this->numByteOffsetBits = log2(ci.blockSize);
	this->numSetIndexBits = log2(ci.numberSets);
	// initialize the counters
	this->globalCycles = 0;
	this->globalHits = 0;
	this->globalMisses = 0;
	this->globalEvictions = 0;
	this->Accessbus = Accessbus;
	this->Localbus = Localbus;
	this->locker = locker;
	this->thread_counter = thread_counter;
	this->cond = cond;	
	// create your cache structure
	// ...
	
	this->cache_structure.resize(this->ci.numberSets);
	for(unsigned int i = 0; i < this->ci.numberSets; i++)
	{
		for(auto j = cache_structure[i].begin(); j != cache_structure[i].end(); ++j)
		{
			cache_block datablock = 
			{	
				0, 
				0,
				0,
			};
			cache_structure[i].push_back(datablock);
			cout << j->valid << endl;
		}	
	}

	// this->cache_structure.resize(this->ci.numberSets);
	// for(unsigned int i= 0; i<this->ci.numberSets; ++i)
	// {
	// 	this->cache_structure[i].resize(this->ci.associativity);
	// 	for(unsigned int j=0; j<this->ci.associativity; j++)
	// 	{
	// 		this->cache_structure[i][j].tag = 0;
	// 		this->cache_structure[i][j].dirty = 0;
	// 		this->cache_structure[i][j].valid = 0;	
	// 		this->cache_structure[i][j].statevariable = static_cast<MESIstate>(3);	
	// 		cout << this->cache_structure[i][j].valid;
	// 	}
	// 	cout << endl;
	// }

	//print the cache
	// std::cout << "Cache Created\n";
	// std::cout << "cache consists of "<<cache_structure.size() << "sets\n";
	
	// for(unsigned int i = 0; i < this->ci.numberSets ; i++)
	// {
	// 	for(unsigned int j = 0; j< this->ci.associativity; j++)
	// 	{
	// 		std::cout << "Index:\tV D Tag\tV D Tag\tV D Tag\t" << std::endl;
	// 		std::cout << i << "\t";
	// 		std::cout << this->cache_structure[i][j].valid;
	// 		std::cout << this->cache_structure[i][j].valid;
	// 		std::cout << this->cache_structure[i][j].valid;
						  
	// 	}
	// 	std::cout << "\n\n";
	// }

	// manual test code to see if the cache is behaving properly
	// will need to be changed slightly to match the function prototype
	/*
	cacheAccess(false, 0);
	cacheAccess(false, 128);
	cacheAccess(false, 256);

	cacheAccess(false, 0);
	cacheAccess(false, 128);
	cacheAccess(false, 256);
	*/
}

/*
	Starts reading the tracefile and processing memory operations.
*/
void CacheController::runTracefile() {
	cout << "Input tracefile: " << inputFile << endl;
	cout << "Output file name: " << outputFile << endl;
	
	// process each input line
	string line;
	// define regular expressions that are used to locate commands
	regex commentPattern("==.*");
	regex instructionPattern("I .*");
	regex loadPattern(" (L )(.*)(,[[:digit:]]+)$");
	regex storePattern(" (S )(.*)(,[[:digit:]]+)$");
	regex modifyPattern(" (M )(.*)(,[[:digit:]]+)$");

	// open the output file
	ofstream outfile(outputFile);
	// open the output file
	ifstream infile(inputFile);
	// parse each line of the file and look for commands
	while (getline(infile, line)) {
		// these strings will be used in the file output
		string opString, activityString;
		smatch match; // will eventually hold the hexadecimal address string
		unsigned long int address;
		// create a struct to track cache responses
		CacheResponse response;
		// ignore comments
		if (std::regex_match(line, commentPattern) || std::regex_match(line, instructionPattern)) {
			// skip over comments and CPU instructions
			cout << "found a comment" << endl;
			continue;
		} else if (std::regex_match(line, match, loadPattern)) {
			cout << "Found a load op!" << endl;
			istringstream hexStream(match.str(2));
			hexStream >> std::hex >> address;
			outfile << match.str(1) << match.str(2) << match.str(3);
			cacheAccess(&response, false, address);
			outfile << " " << response.cycles << (response.hit ? " hit" : " miss") << (response.eviction ? " eviction" : "");
		} else if (std::regex_match(line, match, storePattern)) {
			cout << "Found a store op!" << endl;
			istringstream hexStream(match.str(2));
			hexStream >> std::hex >> address;
			outfile << match.str(1) << match.str(2) << match.str(3);
			cacheAccess(&response, true, address);
			outfile << " " << response.cycles << (response.hit ? " hit" : " miss") << (response.eviction ? " eviction" : "");
		} else if (std::regex_match(line, match, modifyPattern)) {
			cout << "Found a modify op!" << endl;
			istringstream hexStream(match.str(2));
			hexStream >> std::hex >> address;
			outfile << match.str(1) << match.str(2) << match.str(3);
			// first process the read operation
			cacheAccess(&response, false, address);
			string tmpString; // will be used during the file output
			tmpString.append(response.hit ? " hit" : " miss");
			tmpString.append(response.eviction ? " eviction" : "");
			unsigned long int totalCycles = response.cycles; // track the number of cycles used for both stages of the modify operation
			// now process the write operation
			cacheAccess(&response, true, address);
			tmpString.append(response.hit ? " hit" : " miss");
			tmpString.append(response.eviction ? " eviction" : "");
			totalCycles += response.cycles;
			outfile << " " << totalCycles << tmpString;
		} else {
			throw runtime_error("Encountered unknown line format in tracefile.");
		}
		outfile << endl;
	}
	// add the final cache statistics
	outfile << "Hits: " << globalHits << " Misses: " << globalMisses << " Evictions: " << globalEvictions << endl;
	outfile << "Cycles: " << globalCycles << endl;

	infile.close();
	outfile.close();
	lock_guard<mutex> lk(*locker);
	(*this->thread_counter)--;
	this->cond->notify_all();
	}

/*
	Calculate the block index and tag for a specified address.
*/
CacheController::AddressInfo CacheController::getAddressInfo(unsigned long int address) {
	AddressInfo ai;
	// this code should be changed to assign the proper index and tag
	cout << address;
	unsigned long int blockaddress = address / this->ci.blockSize;
	ai.setIndex = blockaddress % this->ci.numberSets;
	ai.tag = blockaddress / this->ci.numberSets;
	return ai;
}

/*
	This function allows us to read or write to the cache.
	The read or write is indicated by isWrite.
*/
void CacheController::cacheAccess(CacheResponse* response, bool isWrite, unsigned long int address) {
	// determine the index and tag
	AddressInfo ai = getAddressInfo(address);

	cout << "\tSet index: " << ai.setIndex << ", tag: " << ai.tag << endl;
	
	// your code needs to update the global counters that track the number of hits, misses, and evictions
	// add your MESI protocol code here
	
	bool block_replaced;
	for(unsigned int i = 0; i < this->ci.associativity; i++)
	{
		for(auto j = cache_structure[i].begin(); j != cache_structure[i].end(); ++j)
		{
			if(j->valid == 1)
			{
				if(ai.tag == j->tag)
				{
					response->hit = true;
					response->eviction = false;
					cache_structure.splice(cache_structure.begin(), cache_structure[ai.setIndex], j);	
					break;
				}
				else
				{
					response->hit = false;
				}		
			}
		}
		for(auto k = cache_structure[i].begin(); j !=cache_structure[i].end();++k)
		{
			if(k->valid == 0)
			{
				(*k).tag = ai.tag;
				(*k).dirty = 0;
				(*k).valid = 1;
				block_replaced = true;
			}
		}
		else if(block_replaced == false && ci.rp == true)
			{	
				perform LRU;
			}
			else if(block_replaced == false && ci.rp == false)
			{
				perform random;
			}
	}
	
	if (response->hit){
		this->globalHits++;
		cout << "Address " << std::hex << address << " was a hit." << endl;
		//two cases: read and write
		//read: increment the hit counter and then pass the data values to cpu 
		//hits, evictions, dirtyevictions ...we are trying to figure out the number of cycles
		// in the case of a read...check if hit or miss...
		//if(hit){calculate the cycles needed to read values from cache, update hitcounter }
		//if(miss){calculate the cycles needed to read values from cache, read required data from memory, update cache and then again read the values from updated cache}

	}
	else{
		this->globalMisses++;
		cout << "Address " << std::hex << address << " was a miss." << endl;
	}

	cout << "-----------------------------------------" << endl;

	return;
}

/*
	Compute the number of cycles used by a particular memory operation.
	This will depend on the cache write policy.
*/
void CacheController::updateCycles(CacheResponse* response, bool isWrite)
{
	// your code should calculate the proper number of cycles
	if(!isWrite)
	{
		if(this->ci.wp == WritePolicy::WriteThrough)
		{	
			if(response->hit)
			{
				response->cycles = this->ci.cacheAccessCycles;//x=number of read cycles to read from cache
			}
			else
			{
				response->cycles = this->ci.cacheAccessCycles + this->ci.memoryAccessCycles;//y=number of read cycles to read from main memory
			}
		}
		else
		{
			if(response->hit)
			{
				response->cycles = this->ci.cacheAccessCycles;//x=number of read cycles to read from cache
			}	
			else
			{
				response->cycles = this->ci.cacheAccessCycles + this->ci.memoryAccessCycles;//y=number of read cycles to read from main memory
			}
		}
		
	}
	else
	{
		if(this->ci.wp == WritePolicy::WriteThrough)
		{
			response->cycles = this->ci.cacheAccessCycles + this->ci.memoryAccessCycles;
		}
		
		else
		{
			if(response->hit)
			{
				response->cycles = this->ci.cacheAccessCycles;
			}
			else
			{
				response->cycles = this->ci.cacheAccessCycles + this->ci.memoryAccessCycles;
			}
		}
		
	}
	this->globalCycles += response->cycles;
}