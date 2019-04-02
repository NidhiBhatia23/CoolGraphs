#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <omp.h>
#include <unistd.h> //For getopts()
#include <getopt.h> //For getopts()
#include <stdbool.h> //For bool

//#define DEBUG
//#define DEBUG_VF
// struct : community
typedef struct comm {
    long size;
    long degree;
} comm;

// struct : edge
typedef struct edge {
    long head;
    long tail;
    double weight;
} edge;

// struct : graph
typedef struct graph {
    long numVertices; /* number of columns */
    long sVertices; /* number of rows - Bipartite graph : number of s vertices */
    /* Handle this later */
    long numEdges; /* Each edge stored twice but counted once */
    long *edgeListPtrs; /* start vertex of edge */
    edge *edgeList; /* end vertex of edge */
} graph;

// struct : clusteringParams
// For storing parameters needed as input
// from the user of the code
typedef struct clusteringParams {
    const char* inFile; // input file
    int fType; // file type
    //bool strongScaling; // enable strong scaling - unsure what to do with this right now
    bool output; // print out the clustering data
    bool VF; // control for turning vertex following on/off
    bool coloring; // control for turning graph coloring on/off
    double C_thresh; // threshold with coloring on
    long minGraphSize; // min |V| to enable coloring
    double threshold; // value of threshold
} clusteringParams;

// function : setDefaultParams
void setDefaultParams(clusteringParams* inputParams) {
    printf("Inside setDefaultParams\n");
    inputParams->fType = 5;
    //inputParams->strongScaling = false;
    inputParams->output = false;
    inputParams->VF = false;
    inputParams->coloring = false;
    inputParams->C_thresh = 0.01;
    inputParams->threshold = 0.000001;
    inputParams->minGraphSize = 100000;
    return;
}

// function : printUsage
void printUsage() {
    printf("Inside printUsage\n");
    printf("***************************************************************************************\n");
    printf("Basic usage: Driver <Options> FileName\n");
    printf("***************************************************************************************\n");
    printf("Input Options: \n");
    printf("***************************************************************************************\n");
    printf("File-type  : -f <1-8>   -- default=7\n");
    printf("File-Type  : (1) Matrix-Market  (2) DIMACS#9 (3) Pajek (each edge once) (4) Pajek (twice) \n");
    printf("           : (5) Metis (DIMACS#10) (6) Simple edge list twice (7) Binary format (8) SNAP\n");
    printf("--------------------------------------------------------------------------------------\n");
    //    printf("Strong scaling : -s         -- default=false\n");
    printf("VF             : -v         -- default=false\n");
    printf("Output         : -o         -- default=false\n");
    printf("Coloring       : -c         -- default=false\n");
    printf("--------------------------------------------------------------------------------------\n");
    printf("Min-size       : -m <value> -- default=100000\n");
    printf("C-threshold    : -d <value> -- default=0.01\n");
    printf("Threshold      : -t <value> -- default=0.000001\n");
    printf("***************************************************************************************\n");
    return;
}

// function : parseInputParams
bool parseInputParams(int numOfArgs, char** stringOfArgs, clusteringParams* inputParams) {
    printf("Inside parseInputParams\n");
    static const char *opt_string = "csvof:t:d:m:";
    int opt = getopt(numOfArgs, stringOfArgs, opt_string);
    while (opt != -1) {
        switch(opt) {
            case 'c' : 
                inputParams->coloring=true;
                break;
                /* Handle this later 
                 * case 's' : inputParams->strongScaling=true;
                 break;
                 */
            case 'v' : 
                inputParams->VF=true;
                break;
            case 'o' : 
                inputParams->output=true;
                break;
            case 'f' : 
                inputParams->fType=atoi(optarg);
                if( (inputParams->fType > 8) || (inputParams->fType < 0) ) {
                    printf("fType must be integer between 1 to 7\n");
                    return false;
                }
                break;
            case 't' :
                inputParams->threshold=atof(optarg);
                if (inputParams->threshold < 0.0) {
                    printf("threshold must be non-negative\n");
                    return false;
                }
                break;
            case 'd' : 
                inputParams->C_thresh=atof(optarg);
                if (inputParams->C_thresh < 0.0) {
                    printf("C_thresh must be non-negative\n");
                    return false;
                }
                break;
            case 'm' : 
                inputParams->minGraphSize=atol(optarg);
                if (inputParams->minGraphSize < 0) {
                    printf("minGraphSize must be non-negative\n");
                    return false;
                }
                break;
            default : 
                fprintf(stderr, "Unknown argument\n");
                printUsage();
                return false;
        }
        opt = getopt(numOfArgs, stringOfArgs, opt_string);
    }
    if ((numOfArgs - optind) != 1) {
        printf("Problem name is not specified\n");
        printUsage();
        return false;
    } else {
        inputParams->inFile=stringOfArgs[optind];
    }

    printf("********************************************\n");
    printf("Input Parameters: \n");
    printf("********************************************\n");
    printf("Input File : %s\n", inputParams->inFile);
    printf("File type : %d\n", inputParams->fType);
    printf("Threshold : %lf\n", inputParams->threshold);
    printf("C-Threshold : %lf\n", inputParams->C_thresh);
    printf("Min graph size : %ld\n", inputParams->minGraphSize);
    printf("--------------------------------------------\n");
    if (inputParams->coloring)
        printf("Coloring : TRUE\n");
    else
        printf("Coloring : FALSE\n");

    /*
       if (inputParams->strongScaling)
       printf("Strong scaling : TRUE\n");
       else
       printf("Strong scaling : FALSE\n");
       */

    if(inputParams->VF)
        printf("VF : TRUE\n");
    else
        printf("VF : FLASE\n");

    if(inputParams->output)
        printf("Output : TRUE\n");
    else
        printf("Output : FALSE\n");

    printf("********************************************\n");
    return true;
}

// function : strPosWithOffset
long strPosWithOffset(char* line, char* delimiter, long offset) {
    char lineArr[strlen(line) - offset];
    strncpy(lineArr, line + offset, strlen(line) - offset);
    char *found = strstr(lineArr, delimiter);
    return(found - lineArr + offset);
}

// function : strPos
long strPos(char* line, char* delimiter) {
    char *found = strstr(line, delimiter);
    return(found - line);
}

// function : subStrStartFromOfLen
void subStrStartFromOfLen(char* str, long pos, long len, char* dest) {
#ifdef DEBUG
    printf("Inside subStrStartFromOfLen\n");
    printf("str : %s\n", str);
    printf("pos : %ld\n", pos);
    printf("len : %ld\n", len);
#endif
    long l = strlen(str) - pos;
    char* strArr = (char*)malloc(l);
    strncpy(strArr, str+pos, l);
    strArr[l] = '\0';
#ifdef DEBUG
    printf("strArr : %s\n", strArr);
#endif
    strncpy(dest, strArr, len);
    dest[len] = '\0';
    free(strArr);
#ifdef DEBUG
    printf("dest : %ld\n", strlen(dest));
#endif
    return;
}

// function : remove_newline_ch
void remove_newline_ch(char *line) {
    int new_line = strlen(line) -1;
    if (line[new_line] == '\n') {
        line[new_line] = '\0';
    }
    return;
}

// function : trimTrailing
void trimTrailing(char * str) {
    long len = strlen(str) - 1;
    if (str[len] == ' ') {
        str[len] = '\0';
    }
}

// function : countTokens
long countTokens(char* line, char* delimiter) {
#ifdef DEBUG
    printf("Inside countTokens\n");
    printf("Input line is .%s.\n", line);
    printf("Input delimiter is .%s.\n", delimiter);
#endif
    // initialize count to 1
    long count = 1;
    long delimiterPos = 0, lastPos = 0;
    long lineLen = strlen(line);
    long delimiterLen = strlen(delimiter);
#ifdef DEBUG
    printf("lineLen is %ld\n", lineLen);
    printf("delimiterLen is %ld\n", delimiterLen);
#endif
    char* tempStr = (char*)malloc(delimiterLen);

    // if lineLen is 0 
    // obviously there are 0 tokens
    if (lineLen == 0) {
        return 0;
    }
    // if delimiterLen is 0
    // obviously there is 1 token
    if (delimiterLen == 0) {
        return 1;
    }

    while(1) {
#ifdef DEBUG
        printf("Inside while loop, delimiterPos is %ld\n", delimiterPos);
#endif
        delimiterPos = strPosWithOffset(line, delimiter, delimiterPos);
#ifdef DEBUG
        printf("Inside while loop, count is %ld\n", count);
        printf("Inside while loop, delimiterPos is %ld\n", delimiterPos);
#endif
        if (delimiterPos == 0) {
            delimiterPos += delimiterLen;
            continue;
        }
        if ( (delimiterPos < 0) || (delimiterPos >= lineLen) ) {
#ifdef DEBUG
            printf("Inside while loop, OMG, count is %ld\n", count);
#endif
            return count;
        }
        //if (delimiterLen != (delimiterPos - lastPos)) {
        long len = delimiterPos - lastPos;
        subStrStartFromOfLen(line, lastPos, len, tempStr);
        if (strcmp(tempStr, delimiter) != 0) {
#ifdef DEBUG
            printf("Inside while loop, OMG2, count is %ld\n", count);
#endif
            count++;
        }
        lastPos = delimiterPos;
        delimiterPos += delimiterLen;
    }
#ifdef DEBUG
    printf("Outside while loop, count is %ld\n", count);
#endif
    free(tempStr);
    return count;
}

// function : getNextToken
long getNextToken(char* line, char* lineMod, char* delimiter) {
#ifdef DEBUG
    printf("Inside getNextToken\n");
    printf("Value of line is %s\n", line);
    printf("Value of lineMod is %s\n", lineMod);
    printf("Value of delimiter is .%s.\n", delimiter);
#endif
    long lineLen = strlen(line);
    long delimiterLen = strlen(delimiter);
#ifdef DEBUG
    printf("lineLen : %ld\n", lineLen);
    printf("delimiterLen : %ld\n", delimiterLen);
#endif
    char* token = (char*)malloc(lineLen);
    char* tempStr = (char*)malloc(delimiterLen);

    if (lineLen == 0) {
        return 0;
    }

    long delimiterPos = strPos(lineMod, delimiter);
#ifdef DEBUG
    printf("delimiterPos : %ld\n", delimiterPos);
#endif
    if (delimiterPos == 0) {
        while(1) {
            subStrStartFromOfLen(lineMod, 0, delimiterLen, tempStr);
            strncpy(tempStr, lineMod, delimiterLen);
#ifdef DEBUG
            printf("tempStr : .%s.\n", tempStr);
#endif
            if (strcmp(tempStr, delimiter) == 0) {
#ifdef DEBUG
                printf("lineMod : .%s.\n", lineMod);
#endif
                long len = strlen(lineMod) - delimiterLen;
                memmove(lineMod, lineMod+delimiterLen, len);
                lineMod[len] = '\0';
#ifdef DEBUG
                printf("lineMod : .%s.\n", lineMod);
#endif
            } else {
#ifdef DEBUG
                printf("breaking out of while loop\n");
#endif
                break;
            }
        }
        delimiterPos = strPos(lineMod, delimiter);
#ifdef DEBUG
        printf("delimiterPos : %ld\n", delimiterPos);
#endif
    }

    if (delimiterPos < 0) {
        long len = strlen(lineMod);
        strncpy(token, lineMod, len);
        token[len] = '\0';
        lineMod[0] = '\0';
        //memset(lineMod, '\0', strlen(lineMod));
        //free(lineMod);
#ifdef DEBUG
        printf("lineMod : .%s.\n", lineMod);
#endif
    } else {
        subStrStartFromOfLen(lineMod, 0, delimiterPos, token);
        long len = strlen(lineMod)-delimiterPos-delimiterLen;
        //strncpy(lineMod, lineMod+delimiterPos+delimiterLen, len);
        memmove(lineMod, lineMod+delimiterPos+delimiterLen, len);
        lineMod[len] = '\0';
        delimiterPos = 0;
        while(1) {
            subStrStartFromOfLen(lineMod, 0, delimiterLen, tempStr);
            if (tempStr == delimiter) {
                long loc = strlen(lineMod)-delimiterLen;
                memmove(lineMod, lineMod+delimiterLen, loc);
                //strncpy(lineMod, lineMod+delimiterLen, loc);
                lineMod[loc] = '\0';
            } else {
                break;
            }
        }
    }
    // free memory allocations
    free(tempStr);
    long ret = atol(token);
    free(token);
#ifdef DEBUG
    printf("getNextToken : returning %ld\n", ret);
#endif
    return ret;
}

// function : findLongestLineSize
long findLongestLineSize(const char* filename) {
    FILE* fin;
    fin = fopen(filename, "r");
    long iCount = 0;
    long maxCount = 0;
    char ch;

    do {
        if ((ch=fgetc(fin))!= EOF && ch!='\n') {
            iCount++;
        } else { 
            if (iCount > maxCount) {
                maxCount = iCount;
            }
            iCount=0;
        }
    } while (ch!=EOF);
    fclose(fin);
    return maxCount;
}

// function : loadMetisFileFormat
// parse file in metis format
bool loadMetisFileFormat(graph *G, const char* filename) {
    printf("Inside loadMetisFileFormat\n");
    long i, j, value, neighbor, mNVer = 0, mNEdge = 0;
    double edgeWeight, vertexWeight;
    size_t len = 0;
    char* myDelimiter = " ";
    char* oneLine;

    long maxLineLen = findLongestLineSize(filename);
#ifdef DEBUG
    printf("maxLineLen : %ld\n", maxLineLen);
#endif

    FILE* fin;
    char comment;
    fin = fopen(filename, "r");
    if (fin == NULL) {
        fprintf(stderr, "Cannot open file %s\n", filename);
        exit(0);
    }

    oneLine = (char*)malloc((maxLineLen+1)*sizeof(char));
    // Ignore comments - line starting with '%'
    do {
        fgets(oneLine, maxLineLen, fin);
        remove_newline_ch(oneLine);
        trimTrailing(oneLine);
        comment = oneLine[0];
    } while (comment == '%' && !feof(fin));

    char* oneLineMod = (char*)malloc(maxLineLen+1);
    memset(oneLineMod, '\0', sizeof(oneLineMod));
    if (oneLineMod) {
        strcpy(oneLineMod, oneLine);
    }
#ifdef DEBUG
    printf("oneLine : %s\n", oneLine);
    printf("oneLineMod : %s\n", oneLineMod);
#endif
    long count = countTokens(oneLineMod, myDelimiter);
#ifdef DEBUG
    printf("Inside loadMetisFileFormat, count is %ld\n", count);
#endif
    if (count) {
        mNVer = getNextToken(oneLine, oneLineMod, myDelimiter);
#ifdef DEBUG
        printf("mNVer : %ld\n", mNVer);
#endif
    }
    count = countTokens(oneLineMod, myDelimiter);
    if (count) {
        mNEdge = getNextToken(oneLine, oneLineMod, myDelimiter);
#ifdef DEBUG
        printf("mNEdge : %ld\n", mNEdge);
#endif
    }
    count = countTokens(oneLineMod, myDelimiter);
    if (count) { 
        value = getNextToken(oneLine, oneLineMod, myDelimiter);
#ifdef DEBUG
        printf("value : %ld\n", value);
#endif
    } else {
        value = 0;
    }
    // free memory allocations
    if (!oneLineMod) {
        free(oneLineMod);
    }

    // Store vertex degree
    long* mVerPtr = (long*)malloc((mNVer+1)*sizeof(long));
    assert(mVerPtr != 0);
    // Store edge information
    edge* mEdgeList = (edge*)malloc((mNEdge*2)*sizeof(edge));
    assert(mEdgeList != 0);

    // Don't understand the point
    //#pragma omp parallel for
    for (i=0; i<=mNVer; i++) {
        mVerPtr[i] = 0; // initialize to 0
    }

    // Don't understand the point
    //#pragma omp parallel for
    for (i=0; i<(2*mNEdge); i++) {
        mEdgeList[i].tail = -1;
        mEdgeList[i].weight = 0;
    }

    // Read rest of the file
    long PtrPos=0, IndPos=0, cumulative=0;
    mVerPtr[PtrPos]=cumulative;
    PtrPos++;
    switch(value) {
        case 0 : // No weights
#ifdef DEBUG
            printf("Input graph has no weights\n");
#endif
            for (i=0; i<mNVer; i++) {
                if(feof(fin)) {
                    fprintf(stderr, "Within function loadMetisFileFormat\n");
                    fprintf(stderr, "Error reading the metis input file\n");
                    fprintf(stderr, "Reached abrupt end\n");
                    // check this! again
                    return false;
                }
                j=0;
                //getline(&oneLine, &len, fin);
                fgets(oneLine, maxLineLen, fin);
#ifdef DEBUG
                printf("oneLine : %s\n", oneLine);
#endif
                remove_newline_ch(oneLine);
                trimTrailing(oneLine);
                if (strlen(oneLine) == 0) {
                    fgets(oneLine, maxLineLen, fin);
                    remove_newline_ch(oneLine);
                    trimTrailing(oneLine);
                }
                char* oneLineMod = (char*)malloc(maxLineLen+1);
                memset(oneLineMod, '\0', sizeof(oneLineMod));
                if (oneLineMod) {
                    strcpy(oneLineMod, oneLine);
                }
#ifdef DEBUG
                printf("oneLine : %s\n", oneLine);
                printf("oneLineMod : %s\n", oneLineMod);
#endif
                count = countTokens(oneLineMod, myDelimiter);
#ifdef DEBUG
                printf("outside while, count is %ld\n", count);
#endif
                while (count) {
#ifdef DEBUG
                    printf("count is %ld\n", count);
#endif
                    neighbor = getNextToken(oneLine, oneLineMod, myDelimiter) - 1; // Zero-based Index
#ifdef DEBUG
                    printf("oneLineMode : %s\n", oneLineMod);
                    printf("neighbor is %ld\n", neighbor);
#endif
                    j++;
                    mEdgeList[IndPos].head = i;
                    mEdgeList[IndPos].tail = neighbor;
                    mEdgeList[IndPos].weight = 1;
                    IndPos++;
                    count = countTokens(oneLineMod, myDelimiter);
#ifdef DEBUG
                    printf("value of count is %ld\n", count);
#endif
                }
#ifdef DEBUG
                printf("value of i is %ld\n", i);
#endif
                if (oneLineMod) {
                    free(oneLineMod);
                }
                cumulative += j;
                mVerPtr[PtrPos] = cumulative;
                PtrPos++;
            }
            break;
    } // End of switch(value)

    // Close the damn file! 
    fclose(fin);
    
    // Populate the graph structure
    G->numVertices = mNVer;
    G->sVertices = mNVer;
    G->numEdges = mNEdge;
    G->edgeListPtrs = mVerPtr;
    G->edgeList = mEdgeList;

    return true;
} // End 

// function : displayGraphCharacteristics
void displayGraphCharacteristics(graph* G) {
#ifdef DEBUG
    printf("Inside displayGraphCharacteristics\n");
#endif
    long sum=0, sum_sq = 0;
    double average, avg_sq, variance, std_dev;
    long maxDegree = 0;
    long isolated = 0;
    long degreeOne = 0;
    long NS = G->sVertices;
    long NV = G->numVertices;
    long NT = NV - NS;
    long NE = G->numEdges;
    long *vtxPtr = G->edgeListPtrs;
    long tNV = NV; // number of vertices

    if ((NS == 0) || (NS == NV)) { // Non-biparite Graph
        for (long i = 0; i < NV; i++) {
            long degree = vtxPtr[i+1] - vtxPtr[i];
            sum_sq += degree*degree;
            sum += degree;
            if (degree > maxDegree) {
                maxDegree = degree;
            }
            if (degree == 0) {
                isolated++;
            }
            if (degree == 1) {
                degreeOne++;
            }
        }
        average = (double)sum/tNV;
        avg_sq = (double)sum_sq/tNV;
        variance = avg_sq - average*average;
        std_dev = sqrt(variance);

        printf("*******************************************\n");
        printf("General Graph: Characteristics :\n");
        printf("*******************************************\n");
        printf("Number of vertices   :  %ld\n", NV);
        printf("Number of edges      :  %ld\n", NE);
        printf("Maximum out-degree is:  %ld\n", maxDegree);
        printf("Average out-degree is:  %lf\n",average);
        printf("Expected value of X^2:  %lf\n",avg_sq);
        printf("Variance is          :  %lf\n",variance);
        printf("Standard deviation   :  %lf\n",std_dev);
        printf("Isolated vertices    :  %ld (%3.2lf%%)\n", isolated, ((double)isolated/tNV)*100);
        printf("Degree-one vertices  :  %ld (%3.2lf%%)\n", degreeOne, ((double)degreeOne/tNV)*100);
        printf("Density              :  %lf%%\n",((double)NE/(NV*NV))*100);
        printf("*******************************************\n");
    }
}

// function : vertexFollowing
long vertexFollowing(graph* G, long* C) {
    printf("Inside vertexFollowing\n");
    long NV = G->numVertices;
    long *vtxPtr = G->edgeListPtrs;
    edge *vtxInd = G->edgeList;
    long numNode = 0;
    clock_t start = clock();

    // Initialize the communities
    // Don't understand the point of it now
    //#pragma omp parallel for  //Parallelize on the outer most loop
    for (long i = 0; i < NV; i++) {
        C[i] = i; // Initialize each vertex to its own cluster
    }

    // Remove isolated and degree-one vertices
    //#pragma omp parallel for
    for (long i = 0; i < NV; i++) {
        long adj1 = vtxPtr[i];
        long adj2 = vtxPtr[i+1];
        if (adj1 == adj2) { // Isolated vertex
            C[i] = -1;
            //__sync_fetch_and_add(&numNode, 1);
            numNode += 1;
        } else if ((adj2 - adj1) == 1) { // Degree one
            // Check if the tail has degree of greater than 1
            long tail = vtxInd[adj1].tail;
            long adj11 = vtxPtr[tail];
            long adj12 = vtxPtr[tail+1];
            if ((adj12 - adj11) > 1 || i > tail) { // Degree of tail is greater than 1
                //__sync_fetch_and_add(&numNode, 1);
                numNode += 1;
                C[i] = tail;
            } // else, chill!
        } 
    } // End of for
    start = clock() - start;
#ifdef DEBUG_VF
    printf("Time to determine number of vertices (numNode) to fix: %lf\n", (double)start/CLOCKS_PER_SEC);
#endif
    return numNode;
}

typedef struct dataItem {
    long data;
    long key;
} dataItem;

// function : hashCode
long hashCode(long key, long size) {
    return(key % size);
}

// function : search
dataItem* search(long key, long size, dataItem** hashArr) {
    // get the hash
    long hashIndex = hashCode(key, size);
    
    // move in array until an empty
    while (hashArr[hashIndex] != NULL) {
        if (hashArr[hashIndex]->key == key) {
            return hashArr[hashIndex];
        }

        // go to the next cell
        ++hashIndex;

        // wrap around the table
        hashIndex = hashIndex % size;
    }
    return NULL;
}

// function : displayHashMap
void displayHashMap(dataItem** hashArr, long size) {
    for (long i = 0; i < size; i++) {
        if (hashArr[i] != NULL) {
            printf("i : %ld = (%ld, %ld)", i, hashArr[i]->key, hashArr[i]->data);
        } else {
            printf("i : %ld = ~~ ", i);
        }
    }
    printf("\n");
}

// function : insert
void insert(long key, long data, long size, dataItem** hashArr) {
    dataItem* item = (dataItem*)malloc(sizeof(dataItem));
    assert(item != 0);
    item->key = key;
    item->data = data;

    // get the hash
    long hashIndex = hashCode(key, size);

    // move in the array until an empty or deleted cell
    while ((hashArr[hashIndex] != NULL) && (hashArr[hashIndex]->key != -1)) {
        // go to the next cell
        ++hashIndex;

        // wrap around the table
        hashIndex = hashIndex % size;
    }
    hashArr[hashIndex] = item;
}

// function : delete
dataItem* delete(dataItem* item, long size, dataItem** hashArr) {
    long key = item->key;

    // get the hash
    long hashIndex = hashCode(key, size);

    // move in array until an empty
    while(hashArr[hashIndex] != NULL) {
        if (hashArr[hashIndex]->key == key) {
            hashArr[hashIndex]->key = -1;
            hashArr[hashIndex]->data = -1;
            return hashArr[hashIndex];
        }
        // go to the next cell
        ++hashIndex;

        // wrap around the table
        hashIndex %= size;
    }
    return NULL;
}

// function : freeHashArr
void freeHashArr(dataItem** hashArr, long size) {
    for (long i = 0; i < size; i++) {
        free(hashArr[i]);
    }
    free(hashArr);
}

// function : renumberClustersContiguously
// WARNING : will overwrite the old cluster
// Returns the number of unique clusters
long renumberClustersContiguously(long* C, long size) {
    printf("Inside renumberClustersContiguously\n");
    clock_t start = clock();
    // count the number of communities and internal edges
    // map each neighbor's cluster to a local number
    dataItem** hashArr = (dataItem**)malloc(size * sizeof(dataItem*));
    for (long i = 0; i < size; i++) {
        hashArr[i] = NULL;
    }
    dataItem* temp = (dataItem*)malloc(sizeof(dataItem));

    long numUniqueClusters = 0;

    // Do this loop in serial
    // will overwrite the old cluster id with new cluster id
    for (long i = 0; i < size; i++) {
        assert(C[i] < size);
        if (C[i] >= 0) { // only if it is a valid number
            temp = search(C[i], size, hashArr); // check if it already exists
            if (temp != NULL) {
                C[i] = temp->data;
            } else {
                insert(C[i], numUniqueClusters, size, hashArr); // Doesn't exist, add to the map
                C[i] = numUniqueClusters; // Renumber the cluster id
                numUniqueClusters++; // increment the number
            }
        }
    } // end of for
    displayHashMap(hashArr, size);
    freeHashArr(hashArr, size);
    free(temp);
    start = clock() - start;
    printf("Time to renumber clusters: %lf\n", (double)start/CLOCKS_PER_SEC);
    return numUniqueClusters; // Return the number of unique cluster ids
}

// function : buildNewGraphVF
// WARNING : will assume that cluster id have been renumbered contiguously
// Return the total time for building the next level of graph
// This will not add any self-loops
double buildNewGraphVF(graph* Gin, graph* Gout, long* C, long numUniqueClusters) {
   // I don't understand the point of this right now
   /*
    int nT;
    #pragma omp parallel
    {
        nT = omp_get_num_threads();
    }
    */
    printf("Inside buildNewGraphVF: # of unique clusters= %ld\n",numUniqueClusters);
    /*
    printf("Actual number of threads: %d \n", nT);
    */
    clock_t start, end; 
    double totTime = 0; // For timing purposes
    double total = 0, totItr = 0;

    // Pointers into the graph structure
    long NV_in = Gin->numVertices;
    long NE_in = Gin->numEdges;
    long* vtxPtrIn = Gin->edgeListPtrs;
    edge* vtxIndIn = Gin->edgeList;

    start = clock();

    // Pointers into the output graph structure
    long NV_out = numUniqueClusters;
    long NE_self = 0; // Not all vertices get self-loops
    long NE_out = 0; // Cross edges
    long* vtxPtrOut = (long*)malloc((NV_out+1)*sizeof(long));
    assert(vtxPtrOut != 0);
    vtxPtrOut[0] = 0; // First location is always 0

    /* Step 1 : Regroup the node into cluster node */
    dataItem*** cluPtrIn = (dataItem***)malloc(numUniqueClusters * sizeof(dataItem**));
    assert(cluPtrIn != 0);
    long* sizeArr = (long*)malloc(numUniqueClusters * sizeof(long));
    assert(sizeArr != 0);

    // Care about it later
    /* 
    #pragma omp parallel for
    */

    // Not sure if needed! Commenting for now
    /*
    for (long i = 0; i < numUniqueClusters; i++) {
        cluPtrIn[i] = malloc(sizeof(dataItem*));
        // Do not add self-loops
        // (*(cluPtrIn[i]))[i] = 0; //Add for a self loop with zero weight
    }
    */

    // Care about it later
    /* 
    #pragma omp parallel for
    */
    for (long i = 1; i <= NV_out; i++) {
        vtxPtrOut[i] = 0;
    }

    // Care about this later
    // Create an array of locks for each cluster
    /*
    omp_lock_t *nlocks = (omp_lock_t *) malloc (numUniqueClusters * sizeof(omp_lock_t));
    assert(nlocks != 0);
    #pragma omp parallel for
    for (long i=0; i<numUniqueClusters; i++) {
        omp_init_lock(&nlocks[i]); //Initialize locks
    }
    */

    end = clock();
    totTime += (double)(end - start)/CLOCKS_PER_SEC;
    printf("Time to initialize: %3.3lf\n", (double)(end - start)/CLOCKS_PER_SEC);
    
    start = clock();
    // Care about it later
    /* 
    #pragma omp parallel for
    */
    for (long i = 0; i < NV_in; i++) {
        if ( (C[i] < 0) || (C[i] > numUniqueClusters) ) {
            continue; // Not a valid cluster id
        }
        long adj1 = vtxPtrIn[i];
        long adj2 = vtxPtrIn[i+1];
#ifdef DEBUG_B
        printf("adj1 : %ld  adj2 : %ld\n", adj1, adj2);
        printf("Community of i : %ld is %ld\n", i, C[i]);
#endif
        dataItem* temp = (dataItem*)malloc(sizeof(dataItem));
        assert(temp != 0);
        long size = 0;
        for (long j = adj1; j < adj2; j++) {
            long tail = vtxIndIn[j].tail;
            assert(C[tail] < numUniqueClusters);
            if (C[i] >= C[tail]) {
                size++;
            }
        }
#ifdef DEBUG_B
        printf("For i=%ld : size is %ld\n", i, size);
#endif
        sizeArr[i] = size;
        cluPtrIn[C[i]] = (dataItem**)malloc(size * (sizeof(dataItem)));
        for (long k = 0; k < size; k++) {
            cluPtrIn[C[i]][k] = NULL;
        }
        // Now look for all the neighbors of this cluster
        for (long j = adj1; j < adj2; j++) {
            long tail = vtxIndIn[j].tail;
            assert(C[tail] < numUniqueClusters);
            // Add the edge from one endpoint
            if (C[i] >= C[tail]) { // Figure out why this condition?
                // Care about this later
                /*
                omp_set_lock(&nlocks[C[i]]);  // Locking the cluster
                */
#ifdef DEBUG_B
                printf("tail for i : %ld, j : %ld is %ld\n", i, j, tail);
                printf("community of tail is C[%ld] : %ld\n", tail, C[tail]);
                printf("Inside for loop j for i=%ld\n", i);
#endif
                temp = search(C[tail], size, cluPtrIn[C[i]]);
                if (temp != NULL) {
                    temp->data += (long)vtxIndIn[j].weight;
                } else {
#ifdef DEBUG_B
                    printf("inside j : %ld and edge weight is vtxIndIn[%ld].weight : %ld\n", j, j, (long)vtxIndIn[j].weight);
#endif
                    insert(C[tail], (long)vtxIndIn[j].weight, size, cluPtrIn[C[i]]); // Inter-community edge
                    //__sync_fetch_and_add(&vtxPtrOut[C[i]+1], 1);
                    vtxPtrOut[C[i]+1] = vtxPtrOut[C[i]+1] + 1;
                    if (C[i] == C[tail]) {
                        // Keep track of self edges
                        //__sync_fetch_and_add(&NE_self, 1);
                        NE_self++;
                    }
                    if (C[i] > C[tail]) {
                        //Keep track of non-self #edges
                        //__sync_fetch_and_add(&NE_out, 1);
                        NE_out++;
                        //Count edge j-->i
                        //__sync_fetch_and_add(&vtxPtrOut[C[tail]+1], 1);
                        vtxPtrOut[C[tail]+1] = vtxPtrOut[C[tail]+1] + 1;
                    }
                }
                /* Dont understand the point of this now
                omp_unset_lock(&nlocks[C[i]]); // Unlocking the cluster
                */
            } // End of if
        } // End of for(j)
#ifdef DEBUG_B
        displayHashMap(cluPtrIn[C[i]], size);
        printf("NE_self : %ld\n", NE_self);
        printf("NE_out : %ld\n", NE_out);
#endif
        free(temp);
    } // End of for(i)
    // Prefix sum:
    for (long i = 0; i < NV_out; i++) {
#ifdef DEBUG_B
        printf("Prev : i : %ld vtxPtrOut[%ld] : %ld ", i, i, vtxPtrOut[i]);
        printf("i+1 : %ld vtxPtrOut[%ld] : %ld ", i+1, i+1, vtxPtrOut[i+1]);
#endif
        vtxPtrOut[i+1] = vtxPtrOut[i+1] + vtxPtrOut[i];    
#ifdef DEBUG_B
        printf("New i : %ld vtxPtrOut[%ld] : %ld    ", i, i, vtxPtrOut[i]);
        printf("New i+1 : %ld vtxPtrOut[%ld] : %ld\n", i+1, i+1, vtxPtrOut[i+1]);
#endif
    }

    end = clock();
    totTime += (double)(end - start)/CLOCKS_PER_SEC;
    printf("NE_out=%ld  NE_self=%ld\n", NE_out, NE_self);
    printf("These should match: %ld == %ld\n", (2*NE_out+NE_self), vtxPtrOut[NV_out]);
    printf("Time to count edges: %3.3lf\n", (double)(end - start)/CLOCKS_PER_SEC);
    assert(vtxPtrOut[NV_out] == (NE_out*2+NE_self)); // Sanity check

    start = clock();
    // Step 3 : build the edge list:
    long numEdges = vtxPtrOut[NV_out];
    long realEdges = NE_out + NE_self; // Self-loops appear once and others appear twice. How?
    edge* vtxIndOut = (edge*)malloc(numEdges * sizeof(edge));
    assert(vtxIndOut != 0);
    long *Added = (long*)malloc(NV_out * sizeof(long)); // Keep track of what got added
    assert(Added != 0);

    // Don't understand the point right now
    //#pragma omp parallel for 
    for (long i = 0; i < NV_out; i++) {
        Added[i] = 0;
    }

    // Now add the edges in no particular order
    // Don't understand the point right now
    //#pragma omp parallel for
    dataItem* temp = (dataItem*)malloc(sizeof(dataItem));
    for (long i = 0; i < NV_out; i++) {
//#ifdef DEBUG_B
        printf("i = %ld\n", i);
//#endif
        long j = 0;
        long Where;
        temp = cluPtrIn[i][j];
        // Now go through the other edges
        while (temp != NULL) {
            // Don't understand the point of this right now
            //Where = vtxPtrOut[i] + __sync_fetch_and_add(&Added[i], 1);
            Where = vtxPtrOut[i] + Added[i]++;
//#ifdef DEBUG_B
            printf("Where : %ld\n", Where);
//#endif
            vtxIndOut[Where].head = i; // Head
            vtxIndOut[Where].tail = temp->key; // Tail
            vtxIndOut[Where].weight = temp->data; // Weight
            if (i != temp->key) {
                // Don't understand the point of this now
                //Where = vtxPtrOut[temp->key] + __sync_fetch_and_add(&Added[i], 1);
                Where = vtxPtrOut[temp->key] + Added[temp->key]++;
                vtxIndOut[Where].head = temp->key;
                vtxIndOut[Where].tail = i; // Tail
                vtxIndOut[Where].weight = temp->data; // Weight
            }
            temp = cluPtrIn[i][j+1];
        } // End of while
    } // End of for(i)
    free(temp);

    end = clock();
    totTime += (double)(end - start)/CLOCKS_PER_SEC;
    printf("Time to build graph: %3.3lf\n", (double)(end - start)/CLOCKS_PER_SEC);
    printf("Total time: %3.3lf\n", totTime);
    printf("Total time to build next phase: %3.3lf\n", totTime);

    // Set the pointers
    Gout->numVertices = NV_out;
    Gout->sVertices = NV_out;
    // Note: Self-loops are represented ONCE, but others appear TWICE
    Gout->numEdges = realEdges; // Add self-loops to the #edges
    Gout->edgeListPtrs = vtxPtrOut;
    Gout->edgeList = vtxIndOut;

    // Clean up
    free(Added);
    // Don't understand the point of it right now
    //#pragma omp parallel for
    for (long i = 0; i < numUniqueClusters; i++) {
        freeHashArr(cluPtrIn[i], sizeArr[i]);    
    }
    free(cluPtrIn);

    // Don't understand the point of it right now
    // #pragma omp parallel for
    /*
    for (long i = 0; i < numUniqueClusters; i++) {
        omp_destroy_lock(&nlocks[i]);
    }
    free(nlocks);
    */

    return(totTime);
} // End of buildNewGraphVF

// function : main
int main(int argc, char** argv) {
    // Step1 : Parse Input Parameters
    //utilityParseInputParameters.hpp
    printf("Inside main\n");
    // Remember to free this!!
    clusteringParams* inputParams = (clusteringParams*)malloc(sizeof(clusteringParams)); 
    setDefaultParams(inputParams);
    bool statusParams = parseInputParams(argc, argv, inputParams);
#ifdef DEBUG
    printf("statusParams is %d\n", statusParams);
#endif
    if (!statusParams) {
        fprintf(stderr, "Could not parse parameters\n");
        free(inputParams);
        return -1;
    }

    // Check for number of threads
    // Dont understand the point of it now
    /*
       int nT = 1; // default number of threads is 1
#pragma omp parallel 
{
nT = omp_get_num_threads();
}
*/
/*
 * I don't understand the point why number of threads need to be more than 1
 * Check if out soon
 if (nT <= 1) {
 fprintf(stderr, "The number of threads should be greater than 1\n");
 return -1;
 }
 */

// Step2 : Parse the input file and store in graph struct
// Remember to free this!!
graph* G = (graph*)malloc(sizeof(graph));
int fType = inputParams->fType;
const char* inFile = inputParams->inFile;
#ifdef DEBUG
printf("Value of fType inside main is : %d\n", fType);
printf("Value of inFile inside main is : %s\n", inFile);
#endif
if (fType == 5) {
    bool readFileStatus = loadMetisFileFormat(G, inFile);
    if (!readFileStatus) {
        fprintf(stderr, "Cannot proceed due to prior mentioned issues in the inputs\n");
        free(G);
        return -1;
    }
}

displayGraphCharacteristics(G);
int coloring = 0;
if (inputParams->coloring) {
    coloring = 1;
}

clock_t start, end;
double cpu_time_used;
// Vertex Following option
if (inputParams->VF) {
    printf("Vertex following is enabled.\n");
    start = clock();
    long numVtxToFix = 0; // Default 0
    long* C = (long*)malloc(G->numVertices*sizeof(long));
    assert(C != 0);
    // Find vertices that follow other vertices
    numVtxToFix = vertexFollowing(G, C);
#ifdef DEBUG_VF
    printf("numVtxToFix : %ld\n", numVtxToFix);
#endif
    if (numVtxToFix > 0) { // Need to fix things : build a new graph
        printf("Graph will be modified -- %ld vertices need to be fixed.\n", numVtxToFix);
        graph *Gnew = (graph *)malloc(sizeof(graph));
        long numClusters = renumberClustersContiguously(C, G->numVertices);    
        double test = buildNewGraphVF(G, Gnew, C, numClusters);
        free(G);
        G = Gnew;
    }
}

// Free the memory space allocated for struct clusteringParams
free(inputParams);
// Free the memory space allocated for struct graph
free(G);
return 0;
}
