#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>
typedef struct Edge_struct
{
	int u;
	int v;
} Edge;
Edge *edgeList;

typedef struct Vertex_data_struct 
{
	int vertexId;
	int vertexParentId;
} VertexData;

Edge initEdge(int u, int v);
int parallelBFS(int numberOfVertex, int numberOfEdge);

int numberOfProcessor, rank;
MPI_Datatype MPI_VERTEX_DATA;
    	
int main (int argc, char *argv[])
{
	//gen data
	int length = 250;
	int s = 0;
	int numberOfVertex = length*length;
	
	int numberOfEdge = 2*length*(length - 1);
	edgeList = malloc(numberOfEdge * sizeof(Edge));
	int salt = 0;
	for (int i = 0; i < numberOfEdge / 2; i++) {
		edgeList[i] = initEdge(i + salt, i + salt + 1);
		//printf("(%2d, %2d)\t", edgeList[i].u, edgeList[i].v);
		if ((i + 1) % (length - 1) == 0) {
		//	printf("\n");
			salt++;
		}
	}

	for (int i = 0; i < numberOfEdge / 2; i++) {
		edgeList[numberOfEdge/2 + i] = initEdge(i, i + length);
		/*printf("(%2d, %2d)\t", edgeList[numberOfEdge/2 + i].u, edgeList[numberOfEdge/2 + i].v);
		if ((i + 1) % length == 0) {
			printf("\n");
		}*/
	}

	/*
	 edgeList[0] = initEdge( 0, 1);	 edgeList[1] = initEdge( 1, 2);	 edgeList[2] = initEdge( 2, 3);
	 edgeList[3] = initEdge( 4, 5);	 edgeList[4] = initEdge( 5, 6);	 edgeList[5] = initEdge( 6, 7);
	 edgeList[6] = initEdge( 8, 9);	 edgeList[7] = initEdge( 9,10);	 edgeList[8] = initEdge(10,11);
	 edgeList[9] = initEdge(12,13);	edgeList[10] = initEdge(13,14);	edgeList[11] = initEdge(14,15);

	edgeList[12] = initEdge( 0, 4);	edgeList[13] = initEdge( 1, 5);	edgeList[14] = initEdge( 2, 6);	edgeList[15] = initEdge( 3, 7);
	edgeList[16] = initEdge( 4, 8);	edgeList[17] = initEdge( 5, 9);	edgeList[18] = initEdge( 6,10);	edgeList[19] = initEdge( 7,11);
	edgeList[20] = initEdge( 8,12);	edgeList[21] = initEdge( 9,13);	edgeList[22] = initEdge(10,14);	edgeList[23] = initEdge(11,15);
	*/
	/********************************************************************************************/

	//define message to transfer
	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcessor);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	int nitems = 2;
	int blocklengths[2] = {1, 1};
	MPI_Datatype types[2] = {MPI_INT, MPI_INT};
    MPI_Aint offsets[2];
    offsets[0] = offsetof(VertexData, vertexId);
    offsets[1] = offsetof(VertexData, vertexParentId);
    MPI_Type_create_struct(nitems, blocklengths, offsets, types, &MPI_VERTEX_DATA);
    MPI_Type_commit(&MPI_VERTEX_DATA);

	double time = 0;
	int loop = 10;
	for (int i = 0; i < loop; i++) {
		int vertexRank[numberOfVertex];
		int vertexParent[numberOfVertex];
		memset(vertexRank, 0, numberOfVertex*sizeof(int));
		memset(vertexParent, -1, numberOfVertex*sizeof(int));
		vertexRank[s] = 1; 	vertexParent[s] = -1;

		double startTime;
		if (rank == 0) {
			printf("new loop\n");
			startTime = MPI_Wtime();
		}
		//BFS
		int visitedVertex = 1;
		int currentVertexRank = 2;
		while(visitedVertex != numberOfVertex) {
			int newFoundParent[numberOfVertex];
			int changeCount = 0;
			memset(newFoundParent, -1, numberOfVertex * sizeof(int));
			for (int i = rank; i < numberOfEdge; i = i + numberOfProcessor)	{
				Edge e = edgeList[i];
				//if rank of u not found and rank of v founded, v is u parent
				if (newFoundParent[e.u] == -1 && vertexRank[e.u] == 0 && vertexRank[e.v] != 0) {
					newFoundParent[e.u] = e.v;
					changeCount++;
					//printf("processor %d found %d parent is %d\n", rank, e.u, e.v);
				} else if (newFoundParent[e.v] == -1 && vertexRank[e.v] == 0 && vertexRank[e.u] != 0) {
					newFoundParent[e.v] = e.u;
					changeCount++;
					//printf("processor %d found %d parent is %d\n", rank, e.v, e.u);
				}
			}
			//printf("processor %d found %d new data\n", rank, changeCount);
			//send data of vertex which found parent 
			MPI_Request request;
			int sendCount = 0;
			for (int i = 0; i < numberOfVertex; i++) {
				if (newFoundParent[i] != -1) {
					VertexData vertexData;
					vertexData.vertexId = i;
					vertexData.vertexParentId = newFoundParent[i];
					//non blocking send
					MPI_Isend(&vertexData, 1, MPI_VERTEX_DATA, 0, 1, MPI_COMM_WORLD, &request);
	      			MPI_Request_free(&request);
	      			sendCount++;
	      			//printf("processor %d send %d data\n", rank, sendCount);
				}
			}
			
			
			//synchronize new found data
			int *changeCountList;
			if (rank == 0) {
				changeCountList = malloc(numberOfProcessor * sizeof(int));
			}

			//printf("Processor %d finnish\n", rank);
			MPI_Gather(&changeCount, 1, MPI_INT, 
					changeCountList, 1, MPI_INT, 
					0, MPI_COMM_WORLD);
			if (rank == 0) {
				//printf("Synchronize\n");
				for (int i = 0; i < numberOfProcessor; i++) {
					//printf("found %d change from processor %d\n", changeCountList[i], i);
					for (int j = 0; j < changeCountList[i]; j++) {
						VertexData vertexData;
						MPI_Recv(&vertexData, 1, MPI_VERTEX_DATA, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

						//change vertex parent and set vertex rank if that vertex haven't visited
						//-> prioritize the ealiest data 
						if (vertexRank[vertexData.vertexId] == 0) {
							vertexRank[vertexData.vertexId] = currentVertexRank;
							vertexParent[vertexData.vertexId] = vertexData.vertexParentId;
							visitedVertex++;
							//printf("accept data from processor %d: vertex %d have parent %d\n", i, vertexData.vertexId, vertexData.vertexParentId);
						} else {
							//printf("denide data from processor %d: vertex %d have parent %d\n", i, vertexData.vertexId, vertexData.vertexParentId);
						}
					}
				}
				free(changeCountList);
				//printf("Current visited vertex %d\n", visitedVertex);
				/*printf("Vertex rank\n");
				for (int i = 0; i < numberOfVertex; i++) {
					printf("%d\t", vertexRank[i]);
					if ((i+1) % 4 == 0) printf("\n");
				}
				printf("Vetex parent\n");
				for (int i = 0; i < numberOfVertex; i++) {
					printf("%d\t", vertexParent[i]);
					if ((i+1) % 4 == 0) printf("\n");
				}
				printf("\n");*/
			}
			
			MPI_Bcast(vertexRank, numberOfVertex, MPI_INT, 0, MPI_COMM_WORLD);
			MPI_Bcast(vertexParent, numberOfVertex, MPI_INT, 0, MPI_COMM_WORLD);
			MPI_Bcast(&visitedVertex, 1, MPI_INT, 0, MPI_COMM_WORLD);
			
			/*if (rank == 1) {
				printf("Vertex rank\n");
				for (int i = 0; i < numberOfVertex; i++) {
					printf("%d\t", vertexRank[i]);
					if ((i+1) % 4 == 0) printf("\n");
				}
				printf("Vetex parent\n");
				for (int i = 0; i < numberOfVertex; i++) {
					printf("%d\t", vertexRank[i]);
					if ((i+1) % 4 == 0) printf("\n");
				}
			}*/
			currentVertexRank++;
		}

		MPI_Barrier(MPI_COMM_WORLD);
		double totalTime;
		if (rank == 0) {
			totalTime = MPI_Wtime() - startTime;
			printf("Done\n");
			printf("Total time: %f\n", totalTime);
			time += totalTime;
			/*printf("Vertex(rank, parent)\n");
			for (int i = 0; i < numberOfVertex; i++) {
				printf("%2d(%2d, %2d)\t", i, vertexRank[i], vertexParent[i]);
				if ((i+1) % 4 == 0) printf("\n");
			}*/
		}
		//MPI_Scatter(edge, edgePerProc, MPI_EDGE, subEdge, edgePerProc, MPI_EDGE, 0, MPI_COMM_WORLD);

		//MPI_Gather(subEdgeList, edgePerProc, MPI_VERTEX_DATA, edgeList, edgePerProc, MPI_VERTEX_DATA, 0, MPI_COMM_WORLD);
	}
	if (rank == 0) printf("Average time: %f\n", time/loop);
	MPI_Finalize();
	return 0;
}

int parallelBFS(int numberOfVertex, int numberOfEdge) {
	
}

Edge initEdge(int u, int v) {
	Edge edgeList;
	edgeList.u = u;
	edgeList.v = v;
	return edgeList;
}
