//Lab Task: resolve "address" to "rank"
// mpicxx labtask2.cpp -o labtask2
// mpirun -np 30 -host localhost ./labtask2

#include <mpi.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>


using namespace std;

#define TAG_NAME_RESOLUTION 	0
#define TAG_NAME_REGISTRATION 	1
#define TAG_NORMAL_MESSAGE		2


//This function is to resolve "address" to "rank" mapping
//In this case, the "address" works like logical address while
//the MPI "rank" is like the physical address
int router (int address)
{
	ifstream routeFile;
	int routeTable[10000];
	routeFile.open ("route", ios::in);
	for (int i=0; i<10000; i++)
		routeFile >> routeTable[i];

	routeFile.close();
	return routeTable[address];
}


int main (int argc, char* argv[]) {
    // Initialize the MPI environment
    MPI_Init (NULL, NULL);

	// The set of all possible names used by the processes
	string name[50] = {"google", "joly", "hp", "inti", "uow", "batman", "ghost", "hydra", "boxy", "porus",
						"samsung", "sony", "apple", "orange", "fish", "cat", "tom", "fatboy", "piper", "danga",
						"yapp", "carson", "june", "may", "sunday", "larry", "happy", "money", "os", "qutie",
						"dummy", "rocky", "xenon", "zebra", "linux", "horse", "klang", "subang", "webber", "zippy",
						"hero", "mapple", "elon", "tron", "ion", "geany", "jess", "vampire", "bbc", "ibm" };


    // Get the number of processes
    // MPI_COMM_WORLD is the predefined "communicator" that includes all MPI processes
    int world_size;
    MPI_Comm_size (MPI_COMM_WORLD, &world_size);

    // Get the rank of the process
    // each process will have a "rank" (just an ID) in the communicator
    int rank;
    MPI_Status stat;
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
	int data = 123;
	int reply;
	int nameServer = 1;	// name server is process 1
	int replicateServer = 2;	//replicate server is process 2
	int serverBusy = 0;		//counter to replicate "busy server"

	// assign name to each node
	string nodeName = name[rank];

	char destNode[20];

	// The struct that holds the name & address pair
	typedef struct {
		char name[20];
		int address;
		int proc;
	} addressStruct;

	// Line 73 to 81 is to create a data type to be used in our message
	int structCount = 3;				// struct has 2 members
	int structLength[3] = {20, 1, 1};		// elements of each member

	MPI_Aint structOffset[3] = {0, 20, 21};	// memory address offset for each member
	MPI_Datatype structDataType[3] = {MPI_CHAR, MPI_INT, MPI_INT};	// The basic data type of each member

	MPI_Datatype addressType;	// The name of our new data type
	MPI_Type_struct (structCount, structLength, structOffset, structDataType, &addressType);	// create the new data type
	MPI_Type_commit (&addressType);


	//Process 0 execute this code
	if (rank == 0) {
		int netAddress;


		sleep (3);	//wait for other process to complete name registration
		while (1) {
			int flag = 0;
			MPI_Request req;
			cout << "\033[31m" << "[P0] Enter name of the destination process : ";
			cin >> destNode;

			//try nameServer (process 1) to resolve destNode
			MPI_Isend(&destNode, strlen(destNode)+1, MPI_CHAR, nameServer, TAG_NAME_RESOLUTION, MPI_COMM_WORLD, &req);

			// Test if primary has received the data
			MPI_Test(&req, &flag, &stat);

			//Send to replicant server if primary has missed it
			if (!flag) {
				cout << "\033[31m" << "Resending to replicant..." << endl;
				MPI_Send (&destNode, strlen(destNode)+1, MPI_CHAR, replicateServer, TAG_NAME_RESOLUTION, MPI_COMM_WORLD);
				MPI_Recv (&netAddress, 1, MPI_INT, replicateServer, TAG_NAME_RESOLUTION, MPI_COMM_WORLD, &stat);
			} else {
				//receive reply (address) from nameServer
				MPI_Recv(&netAddress, 1, MPI_INT, nameServer, TAG_NAME_RESOLUTION, MPI_COMM_WORLD, &stat);
			}

			//if the destName is valid
			if (netAddress >= 0) {
				//Then use address to communicate with the destination process
				cout << "\033[33m" << "[P0] Sending data to process \"" << destNode << "\" at address " << netAddress << endl;
				MPI_Send (&data, 1, MPI_INT, router(netAddress), TAG_NORMAL_MESSAGE, MPI_COMM_WORLD);
				MPI_Recv (&reply, 1, MPI_INT, router(netAddress), TAG_NORMAL_MESSAGE, MPI_COMM_WORLD, &stat);
				cout << "\033[32m" << "[P0] Receiving reply from \"" << destNode << "\"" << endl;
			} else {
				cout << "\033[31m" << "[P0] Invalid process name" << endl;
			}

		}
	} else if (rank == 1) { //Process 1 act as the name resolution server
			// to store routing information
			int routeTable[10000];
			for (int i=0; i<10000; i++)
				routeTable[i] = 0;
			ofstream routeFile;
			routeFile.open ("route", ios::out);


			// to store name/address pair
			addressStruct database[world_size];
			addressStruct regName;
			int src;

			//receive name registration from all processes (except P0)
			//and save them in the "database"
			for (int a=3; a<world_size; a++) {
				MPI_Recv (&regName, 1, addressType, MPI_ANY_SOURCE, TAG_NAME_REGISTRATION, MPI_COMM_WORLD, &stat);
				// use rank as index of the database
				src = stat.MPI_SOURCE;
				database[src] = regName;
				//Debug print
				cout << regName.name << "\t" << regName.address << "\t" << src << endl;

				// send address to replicant server
				MPI_Send (&regName, 1, addressType, replicateServer, 2, MPI_COMM_WORLD);


				// add record to routing table as well
				routeTable[regName.address] = src;	//for "internal routing" purposes, you can ignore this line
			}

			// save routing table to file
			for (int i=0; i<10000; i++)
				routeFile << routeTable[i] << endl;

			routeFile.close();

			cout << "\033[32m" << "[Name Server] Name registration completed" << endl;

			// waiting for query
			while (1) {
				int result = -1;
				//receive name query from P0
				MPI_Recv (&destNode, 20, MPI_CHAR, 0, TAG_NAME_RESOLUTION, MPI_COMM_WORLD, &stat);
				// Lookup destNode from database
				for (int a=1; a<world_size; a++) {
					if ( strcmp (destNode, database[a].name) == 0 ) {
						result = database[a].address;
						break;
					}
				}
				// send back result to P0
				MPI_Send (&result, 1, MPI_INT, 0, TAG_NAME_RESOLUTION, MPI_COMM_WORLD);
			}

	} else if (rank == 2) { //Process 2 act as replicant server
		addressStruct database[world_size];
		addressStruct regName;
		int src;

		for (int a=3; a<world_size; a++) {
			// receive address from nameServer
			MPI_Recv (&regName, 1, addressType, nameServer, 2, MPI_COMM_WORLD, &stat);
			//src = regName.proc;
			//store rank address
			database[regName.proc] = regName;
			//cout << "[2] " << regName.name << "\t" << regName.address << "\t" << regName.proc << endl;
		}

		// waiting for query
		while (1) {
				int result = -1;
				//receive name query from P0
				MPI_Recv (&destNode, 20, MPI_CHAR, 0, TAG_NAME_RESOLUTION, MPI_COMM_WORLD, &stat);
				// Lookup destNode from database
				for (int a=1; a<world_size; a++) {
					if (strcmp (destNode, database[a].name) == 0 ) {
						result = database[a].address;
						break;
					}
				}
				// send back result to P0
				MPI_Send (&result, 1, MPI_INT, 0, TAG_NAME_RESOLUTION, MPI_COMM_WORLD);
			}

	} else {
		// Other processes execute this code
		addressStruct myNameAddress;

		strcpy (myNameAddress.name, nodeName.c_str());
		srand (rank*5);
		myNameAddress.address = rand() % 9000 + 1000;	//Network address of each node range from 1000 to 9999
		myNameAddress.proc = rank;


		// Register name/address pair to the name server
		MPI_Send (&myNameAddress, 1, addressType, nameServer, TAG_NAME_REGISTRATION, MPI_COMM_WORLD);
		//cout << myNameAddress.name << "\t" << myNameAddress.address << "\t" << myNameAddress.proc << endl;
		// waiting for P0
		while (1) {
			//receive request from peer 0
			MPI_Recv (&data, 1, MPI_INT, 0, TAG_NORMAL_MESSAGE, MPI_COMM_WORLD, &stat);
			cout << "\033[34m" << "[" << nodeName << "] received request from P0" << endl;
			reply = 456;
			//send back the reply
			MPI_Send (&reply, 1, MPI_INT, 0, TAG_NORMAL_MESSAGE, MPI_COMM_WORLD);
		}
	}
    // Finalize the MPI environment.
    MPI_Finalize();
}
