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
#include "RngStream.h"

#define MaxDegree 4096 // Increase if number of colors is larger, decrease if memory is not enough

//#define DEBUG
//#define DEBUG_VF
//#define DEBUG_SEARCH

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
    //printf("Inside setDefaultParams\n");
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
    //printf("Inside printUsage\n");
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
        //clock_t start = clock();
        double start = omp_get_wtime();

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
        start = omp_get_wtime() - start;
        //printf("Time to determine number of vertices (numNode) to fix: %lf\n", (double)start/CLOCKS_PER_SEC);
        printf("Time to determine number of vertices (numNode) to fix: %3.3lf\n", start);
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
#ifdef DEBUG_SEARCH
        printf("Inside search : key=%ld size=%ld\n", key, size);
#endif
        long hashIndex = hashCode(key, size);
#ifdef DEBUG_SEARCH
        printf("hashIndex : %ld\n", hashIndex);
#endif

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
        //printf("key : %ld   data : %ld\n", key, data);
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
        //printf("Inside insert : key : %ld   data : %ld\n", hashArr[hashIndex]->key, hashArr[hashIndex]->data);
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
        //clock_t start = clock();
        double start = omp_get_wtime();
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
        //displayHashMap(hashArr, size);
        freeHashArr(hashArr, size);
        //free(temp);
        start = omp_get_wtime() - start;
        printf("Time to renumber clusters: %3.3lf\n", start);
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
    //clock_t start, end; 
    double start, end; 
    double totTime = 0; // For timing purposes
    double total = 0, totItr = 0;

    // Pointers into the graph structure
    long NV_in = Gin->numVertices;
    long NE_in = Gin->numEdges;
    long* vtxPtrIn = Gin->edgeListPtrs;
    edge* vtxIndIn = Gin->edgeList;

    start = omp_get_wtime();

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

end = omp_get_wtime();
totTime += (end - start);
printf("Time to initialize: %3.3lf\n", (end - start));

start = omp_get_wtime();
// Care about it later
/* 
#pragma omp parallel for
*/

//size for community hash
for (long i = 0; i < NV_in; i++) {
    if ( (C[i] < 0) || (C[i] > numUniqueClusters) ) {
        continue; // Not a valid cluster id
    }
    long adj1 = vtxPtrIn[i];
    long adj2 = vtxPtrIn[i+1];
    for (long j = adj1; j < adj2; j++) {
        long tail = vtxIndIn[j].tail;
        assert(C[tail] < numUniqueClusters);
        if (C[i] > C[tail]) {
            sizeArr[C[i]]++;
        }
        if (C[i] == C[tail] && i <= tail) {
            sizeArr[C[i]]++;
        }
    }
    cluPtrIn[C[i]] = (dataItem**)malloc(sizeArr[C[i]] * (sizeof(dataItem)));
    for (long k = 0; k < sizeArr[C[i]]; k++) {
        cluPtrIn[C[i]][k] = NULL;
    }
}

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
    /*
       long size = 0;
       for (long j = adj1; j < adj2; j++) {
       long tail = vtxIndIn[j].tail;
       assert(C[tail] < numUniqueClusters);
       if (C[i] >= C[tail]) {
       size++;
       }
       }
       */
#ifdef DEBUG_B
    printf("For i=%ld : size is %ld\n", i, sizeArr[C[i]]);
#endif
    //sizeArr[i] = size;
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
            printf("community of i=%ld is %ld\n", i, C[i]);
#endif
            temp = search(C[tail], sizeArr[C[i]], cluPtrIn[C[i]]);
#ifdef DEBUG_B
            printf("temp is %p\n", temp);

#endif
            //printf("Dude\n");
            //printf("sizeArr[0] : %ld\n", sizeArr[0]);
            //displayHashMap(cluPtrIn[0], sizeArr[0]);
            //printf("Dude\n");
            if (temp != NULL) {
                //displayHashMap(cluPtrIn[C[i]], sizeArr[C[i]]);
                temp->data += (long)vtxIndIn[j].weight;
                //displayHashMap(cluPtrIn[C[i]], sizeArr[C[i]]);
                //printf("C[i] is %ld\n", C[i]);
                //printf("C[tail] is %ld\n", C[tail]);
                //printf("i is %ld\n", i);
                //printf("temp->data : %ld\n", temp->data);
            } else {
#ifdef DEBUG_B
                printf("inside j : %ld and edge weight is vtxIndIn[%ld].weight : %ld\n", j, j, (long)vtxIndIn[j].weight);
#endif
                insert(C[tail], (long)vtxIndIn[j].weight, sizeArr[C[i]], cluPtrIn[C[i]]); // Inter-community edge
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
    //displayHashMap(cluPtrIn[C[i]], sizeArr[C[i]]);
#ifdef DEBUG_B
    displayHashMap(cluPtrIn[C[i]], sizeArr[C[i]]);
    printf("NE_self : %ld\n", NE_self);
    printf("NE_out : %ld\n", NE_out);
#endif
    //free(temp);
} // End of for(i)
/*
for (long n = 0; n < NV_out; n++) {
    displayHashMap(cluPtrIn[n], sizeArr[n]);
}
*/
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

end = omp_get_wtime();
totTime += (end - start);
printf("NE_out=%ld  NE_self=%ld\n", NE_out, NE_self);
printf("These should match: %ld == %ld\n", (2*NE_out+NE_self), vtxPtrOut[NV_out]);
printf("Time to count edges: %3.3lf\n", (end - start));
assert(vtxPtrOut[NV_out] == (NE_out*2+NE_self)); // Sanity check

start = omp_get_wtime();
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
#ifdef DEBUG_B
    printf("i = %ld\n", i);
#endif
    long j = 0;
    long Where;
    temp = cluPtrIn[i][j];
    //printf("i : %ld j : %ld\n",  i, j);
    //printf("sizeArr[i] : %ld\n", sizeArr[i]);
    //printf("temp->data : %ld\n", temp->data);
    //displayHashMap(cluPtrIn[i], sizeArr[i]);
    // Now go through the other edges
    while (j < sizeArr[i]) {
        // Don't understand the point of this right now
        //Where = vtxPtrOut[i] + __sync_fetch_and_add(&Added[i], 1);
        Where = vtxPtrOut[i] + Added[i]++;
#ifdef DEBUG_B
        printf("Where : %ld\n", Where);
#endif
        vtxIndOut[Where].head = i; // Head
#ifdef DEBUG_B
        printf("head : %ld\n", i);
#endif
        vtxIndOut[Where].tail = temp->key; // Tail
#ifdef DEBUG_B
        printf("tail : %ld\n", temp->key);
#endif
        vtxIndOut[Where].weight = temp->data; // Weight
#ifdef DEBUG_B
        printf("head : %ld  tail : %ld  weight : %ld\n", i, temp->key, temp->data);
#endif
        if (i != temp->key) {
            // Don't understand the point of this now
            //Where = vtxPtrOut[temp->key] + __sync_fetch_and_add(&Added[i], 1);
            Where = vtxPtrOut[temp->key] + Added[temp->key]++;
            vtxIndOut[Where].head = temp->key;
            vtxIndOut[Where].tail = i; // Tail
            vtxIndOut[Where].weight = temp->data; // Weight
        }
        j++;
        temp = cluPtrIn[i][j];
    } // End of while
} // End of for(i)
free(temp);

end = omp_get_wtime();
totTime += (end - start);
printf("Time to build graph: %3.3lf\n", (end - start));
printf("Total time: %3.3lf\n", totTime);
//printf("Total time to build next phase: %3.3lf\n", totTime);

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

// function : generateRandomNumbers
void generateRandomNumbers(double* RandVec, long size) {
    printf("Inside generateRandomNumbers\n");
    int nT = 1;
    // Don't understand the point of this right now
    //int nT;
    //#pragma omp parallel
    //{
    //  nT = omp_get_num_threads();
    //}
    //printf("Number of threads: %d\n", nT);

    // Initialize parallel pseudo-random number generator
    unsigned long seed[6] = {1, 2, 3, 4, 5, 6};
    RngStream_SetPackageSeed(seed);
    RngStream RngArray[nT]; //array of RngStream Objects

    long block = size/nT;
    printf("Each thread will add %ld edges\n", block);

    // Each thread will generate m/nT edges each
    //double start = omp_get_wtime();
    //clock_t start = clock();

    // Don't understand the point of this right now
    // #pragma omp parallel
    // {
    //  int myRank = omp_get_thread_num();
    //  #pragma omp for schedule(static)
    //  for (long i = 0; i < size; i++) {
    //      RandVec[i] = RndArray[myRank].RandU01();
    //  }
    // }

    for (int i = 0; i < nT; i++) {
        RngArray[i] = RngStream_CreateStream("");
    }

    int myRank = 0;
    //printf("myRank=%d\n", myRank);
    for (long i = 0; i < size; i++) {
        //printf("i=%ld\n", i);
        RandVec[i] = RngStream_RandU01(RngArray[myRank]);
    } 
} // End of generateRandomNumbers

// function : algoDistanceOneVertexColoringOpt
int algoDistanceOneVertexColoringOpt(graph* G, int* vtxColor, int nThreads, double* totTime) {
    printf("Inside algoDistanceOneVertexColoringOpt\n");
    // Don't understand the point of this right now
    /*
       if (nThreads < 1) {
       omp_set_num_threads(1); // default to one thread
       } else {
       omp_set_num_threads(nThreads);
       }

       int nT;
#pragma omp parallel
{
nT = omp_get_num_threads();
}
printf("Actual number of threads: %d (requested: %d)\n", nT, nThreads);
*/

    double start, end;
    double totalTime=0;
    // Get the iterators for the graph:
    long NVer = G->numVertices;
    long NEdge = G->numEdges;
    long* vtxPtr = G->edgeListPtrs; // vertex pointer: pointers to endV
    edge* vtxInd = G->edgeList; // vertex index : destination id of an edge (src -> dest)
    //printf("Vertices : %ld  Edges : %ld\n", NVer, NEdge);

    // Build a vector of random numbers
    double* randValues = (double*)malloc(NVer * sizeof(double));
    assert(randValues != 0);
    generateRandomNumbers(randValues, NVer);
    /*
    for (long i = 0; i < NVer; i++) {
        printf("i=%ld   %lf ", i, randValues[i]);
    }
printf("\n");
*/

long* Q = (long*)malloc(NVer * sizeof(long));
assert(Q != 0);
long* Qtmp = (long*)malloc(NVer * sizeof(long));
assert(Qtmp != 0);
long* Qswap;
if ((Q == NULL) || (Qtmp == NULL)) {
    printf("Not enough memory to allocate two queues\n");
    exit(1);
}

long QTail = 0; // Tail of the queue
long QtmpTail = 0; // Tail of the queue (implicitly will represent the size)

// Don't understand the point of this right now
// #pragma omp parallel for
for (long i = 0; i < NVer; i++) {
    Q[i] = i; // Natural order
    Qtmp[i] = -1; // empty queue
}
QTail = NVer; // Queue all vertices
//////////////////////////////////////////////////////////////////////////////
////////////////// START THE WHILE LOOP //////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
long nConflicts = 0; // Number of conflicts
int nLoops = 0; // Number of rounds of conflict resolution

printf("Results from parallel coloring:\n");
printf("***********************************\n");

do {
    /////////////////////// PART 1 /////////////////////
    // Color the vertices in parallel - do not worry about conflicts
    printf("** Iteration : %d\n", nLoops);

    start = omp_get_wtime();
    // Don't understand the point of this right now
    //#pragma omp parallel for
    for (long Qi = 0; Qi < QTail; Qi++) {
        long v = Q[Qi]; // Q.pop_front();

        long adj1 = vtxPtr[v];
        long adj2 = vtxPtr[v+1];
        long myDegree = adj2 - adj1;
        bool* Mark = (bool*)malloc(MaxDegree * sizeof(bool));
        assert(Mark != 0);

        for (long i = 0; i < MaxDegree; i++) {
            Mark[i] = false;
        }

        int maxColor = -1;
        int adjColor = -1;

        // Browse the adjacency set of vertex v
        for (long k = adj1; k < adj2; k++) {
            if (v == vtxInd[k].tail) { // Self-loops
                continue;
            }
            adjColor = vtxColor[vtxInd[k].tail];
            if (adjColor >= 0) {
                assert(adjColor < MaxDegree);
                Mark[adjColor] = true;
                // Find the largest color in the neighborhood
                if (adjColor > maxColor) {
                    maxColor = adjColor;
                }
            }
        } // End of for loop to traverse adjacency of v

        int myColor;
        for (myColor = 0; myColor <= maxColor; myColor++) {
            if (Mark[myColor] == false) {
                break;    
            }
        }

        if (myColor == maxColor) {
            myColor++; // no available color with # less then cmax
        }

        vtxColor[v] = myColor; // Color the vertex

        free(Mark);
    } // End of outer for loop : for each vertex
    start = omp_get_wtime() - start;
    totalTime += (start);
    printf("Time taken for coloring: %lf sec.\n", start);
    /*
    for (int u = 0; u < NVer; u++) {
        printf(" vtxColor[%ld] :: %ld,", u, vtxColor[u]);
    }
    printf("\n");
    */

    //////////////////////// PART 2 /////////////////////////
    // Detect conflicts
    printf("Phase 2 : Detect conflicts, add to Queue\n");
    // Add the conflicting vertices into a Q:
    // Conflicts are resolved by changing the color of only one of 
    // the two conflicting vertices, based on their random values
    end = omp_get_wtime();
    // Don't understand the point of this
    // #pragma omp parallel for
    for (long Qi = 0; Qi < QTail; Qi++) {
        long v = Q[Qi]; // Q.pop_front();
        long adj1 = vtxPtr[v];
        long adj2 = vtxPtr[v+1];
        // Browse the adjacency set of vertex v
        for (long k = adj1; k < adj2; k++) {
            if (v == vtxInd[k].tail) { // Self-loops
                continue;
            }
            if (vtxColor[v] == vtxColor[vtxInd[k].tail]) {
                if ( (randValues[v] < randValues[vtxInd[k].tail]) || 
                        ( (randValues[v] == randValues[vtxInd[k].tail]) && (v < vtxInd[k].tail) ) ) {
                    //long whereInQ = __sync_fetch_and_add(&QtmpTail, 1);
                    long whereInQ = QtmpTail;
                    QtmpTail++;
                    Qtmp[whereInQ] = v; // Add to the Queue
                    vtxColor[v] = -1; // Will prevent v from being in conflict in another pairing
                    break;
                } 
            } // End of if (vtxColor[v] == vtxColor[vtxInd[k].tail])
        } // End of inner for loop: w in adj(v)
    } // //End of outer for loop: for each vertex

    end = omp_get_wtime() - end;
    totalTime += (end);
    nConflicts += QtmpTail;
    nLoops++;
    printf("Num conflicts : %ld\n", QtmpTail);
    printf("Time for detection : %lf sec\n", (end));

    // Swap the two queues:
    Qswap = Q;
    Q = Qtmp; // Q now points to the second vector
    Qtmp = Qswap; 
    QTail = QtmpTail; // Number of elements
    QtmpTail = 0; // Symbolic emptying of the second queue
} while (QTail > 0);

// Check the number of colors used
int nColors = -1;
for (long v = 0; v < NVer; v++) {
    //printf(" vtxColor[%ld] : %ld,", v, vtxColor[v]);
    if (vtxColor[v] > nColors) {
        nColors = vtxColor[v];
    }
}
printf("\n");
printf("***************************************\n");
printf("Total number of colors used: %d\n", nColors);
printf("Number of conflicts overall: %ld\n", nConflicts);
printf("Number of rounds: %d\n", nLoops);
printf("Total time: %lf sec\n", totalTime);
printf("***************************************\n");
*totTime = totalTime;
//////////////////////////////////////////////////////////////////
////////////// VERIFY THE COLORS /////////////////////////////////
//////////////////////////////////////////////////////////////////
// Verify results and cleanup
int myConflicts = 0;
// Don't understand the point of this right now
// #pragma omp parallel for
for (long v = 0; v < NVer; v++) {
    long adj1 = vtxPtr[v];
    long adj2 = vtxPtr[v+1];
    // Browse the adjacency set of vertex v
    for (long k = adj1; k < adj2; k++) {
        if (v == vtxInd[k].tail) {
            continue; // Self-loops
        }
        if (vtxColor[v] == vtxColor[vtxInd[k].tail]) {
            //__sync_fetch_and_add(&myConflicts, 1); //increment the counter
            myConflicts++;
        }
    } //End of inner for loop: w in adj(v)
} //End of outer for loop: for each vertex
myConflicts = myConflicts / 2; // Have counted each conflict twice
if (myConflicts > 0) {
    printf("Check - WARNING: Number of conflicts detected after resolution: %d \n\n", myConflicts);
} else {
    printf("Check - SUCCESS: No conflicts exist\n\n");
}
// Clean-up
free(Q);
free(Qtmp);
free(randValues);

return nColors; // Return the number of colors used
}

void sumVertexDegree(edge* vtxInd, long* vtxPtr, long* vDegree, long NV, comm* cInfo) {
    //#pragma omp parallel for
    for (long i=0; i<NV; i++) {
        long adj1 = vtxPtr[i];      //Begining
        long adj2 = vtxPtr[i+1];    //End
        long totalWt = 0;
        for(long j=adj1; j<adj2; j++) {
            totalWt += (long)vtxInd[j].weight;
        }
        vDegree[i] = totalWt;       //Degree of each node
        cInfo[i].degree = totalWt;  //Initialize the community
        cInfo[i].size = 1;
    }
} //End of sumVertexDegree()

double calConstantForSecondTerm(long* vDegree, long NV) {
    long totalEdgeWeightTwice = 0;
    //#pragma omp parallel
    //{
    long localWeight = 0;
    //#pragma omp for
    for (long i=0; i<NV; i++) {
        localWeight += vDegree[i];
    }
    //#pragma omp critical
    //{
        totalEdgeWeightTwice += localWeight; //Update the global weight
    //}
    //} // End the parallel region
    return 1/(double)totalEdgeWeightTwice;
} //End of calConstantForSecondTerm()

void initCommAss(long* pastCommAss, long* currCommAss, long NV) {
    //#pragma omp parallel for
    for (long i=0; i<NV; i++) {
        pastCommAss[i] = i; //Initialize each vertex to its cluster
        currCommAss[i] = i;
    }
} //End of initCommAss()

long buildLocalMapCounter(long adj1, long adj2, dataItem** clusterLocalMap,
        double* Counter, edge* vtxInd, long* currCommAss, long me, long NV) {
    long size = NV;
    dataItem* temp = (dataItem*)malloc(sizeof(dataItem));
    long numUniqueClusters = 1;
    long selfLoop = 0;
    for(long j=adj1; j<adj2; j++) {
        if(vtxInd[j].tail == me) {  // SelfLoop need to be recorded
            selfLoop += (long)vtxInd[j].weight;
        }
        temp = search(currCommAss[vtxInd[j].tail], size, clusterLocalMap); // check if it already exists
        if (temp != NULL) { // Already exists
            //printf("I am here\n");
            //printf("temp->data : %ld ,  Counter[temp->data] : %lf\n",  temp->data,Counter[temp->data]);
            //printf("vtxInd[j].weight : %lf\n", vtxInd[j].weight);
            Counter[temp->data] += vtxInd[j].weight; //Increment the counter with weight
            //printf("temp->data : %ld ,  Counter[temp->data] : %lf\n",  temp->data,Counter[temp->data]);
        } else {
            insert(currCommAss[vtxInd[j].tail], numUniqueClusters, size, clusterLocalMap); // Doesn't exist, add to the map
            Counter[numUniqueClusters] = vtxInd[j].weight;
            numUniqueClusters++;
        }
    } //End of for(j)
    //printf("returning %lf\n", Counter[0]);
    //printf("returning %ld\n", (long)Counter[0]);
    //printf("returing selfloop : %ld\n", selfLoop);
    return selfLoop;
} //End of buildLocalMapCounter()

long max(dataItem** clusterLocalMap, double* Counter,
        long selfLoop, comm* cInfo, long degree, long sc, double constant, long size) {
    dataItem* temp = (dataItem*)malloc(sizeof(dataItem));
    long maxIndex = sc;   //Assign the initial value as self community
    double curGain = 0;
    double maxGain = 0;
    double eix = Counter[0] - selfLoop;
    double ax = cInfo[sc].degree - degree;
    double eiy = 0;
    double ay = 0;
    
    long j = 0;
    temp = clusterLocalMap[0];
    //displayHashMap(clusterLocalMap, size);
    //printf("I am sc : %ld\n", sc);
    while (j < size) {
        if(temp != NULL && sc != temp->key) {
            ay = cInfo[temp->key].degree; // degree of cluster y
            eiy = Counter[temp->data];     //Total edges incident on cluster y
            curGain = 2*(eiy - eix) - 2*degree*(ay - ax)*constant;
            if( (curGain > maxGain) || ((curGain==maxGain) && (curGain != 0) 
                        && (temp->key < maxIndex)) ) {
                maxGain = curGain;
                maxIndex = temp->key;
            }
        }
        j++;
        temp = clusterLocalMap[j];
    } // End of while

    if(cInfo[maxIndex].size == 1 && cInfo[sc].size ==1 && maxIndex > sc) { //Swap protection
        maxIndex = sc;
    }

    return maxIndex;
} //End max()

double parallelLouvianMethod(graph *G, long *C, int nThreads, double Lower,
        double thresh, double *totTime, int *numItr) {
    printf("Within parallelLouvianMethod()\n");
    /*
    if (nThreads < 1) {
        omp_set_num_threads(1);
    } else {
        omp_set_num_threads(nThreads);
    }
    */
    int nT;
    //#pragma omp parallel
    //{
        nT = omp_get_num_threads();
    //}
    printf("Actual number of threads: %d (requested: %d)\n", nT, nThreads);
    double time1, time2, time3, time4; //For timing purposes
    double total = 0, totItr = 0;
    long    NV        = G->numVertices;
    long    NS        = G->sVertices;  
    long    NE        = G->numEdges;
    long    *vtxPtr   = G->edgeListPtrs;
    edge    *vtxInd   = G->edgeList;
    
    /* Variables for computing modularity */
    long totalEdgeWeightTwice;
    double constantForSecondTerm;
    double prevMod=-1;
    double currMod=-1;
    //double thresMod = 0.000001;
    double thresMod = thresh; //Input parameter
    int numItrs = 0;

    /********************** Initialization **************************/
    time1 = omp_get_wtime();
    //Store the degree of all vertices
    long* vDegree = (long *) malloc (NV * sizeof(long)); 
    assert(vDegree != 0);
    //Community info. (ai and size)
    comm *cInfo = (comm *) malloc (NV * sizeof(comm)); 
    assert(cInfo != 0);
    //use for updating Community
    comm *cUpdate = (comm*)malloc(NV*sizeof(comm)); 
    assert(cUpdate != 0);
    //use for Modularity calculation (eii)
    long* clusterWeightInternal = (long*) malloc (NV*sizeof(long)); 
    assert(clusterWeightInternal != 0);
    sumVertexDegree(vtxInd, vtxPtr, vDegree, NV , cInfo); // Sum up the vertex degree
    /*** Compute the total edge weight (2m) and 1/2m ***/
    constantForSecondTerm = calConstantForSecondTerm(vDegree, NV); // 1 over sum of the degree
    //Community assignments:
    //Store previous iteration's community assignment
    long* pastCommAss = (long *) malloc (NV * sizeof(long)); 
    assert(pastCommAss != 0);
    //Store current community assignment
    long* currCommAss = (long *) malloc (NV * sizeof(long)); 
    assert(currCommAss != 0);
    //Store the target of community assignment
    long* targetCommAss = (long *) malloc (NV * sizeof(long)); 
    assert(targetCommAss != 0);

    //Initialize each vertex to its own cluster
    initCommAss(pastCommAss, currCommAss, NV);

    time2 = omp_get_wtime();
    printf("Time to initialize: %3.3lf\n", time2-time1);

    printf("========================================================================================================\n");
    printf("Itr      E_xx            A_x2           Curr-Mod         Time-1(s)       Time-2(s)        T/Itr(s)\n");
    printf("========================================================================================================\n");

    /*

    printf("=====================================================\n");
    printf("Itr      Curr-Mod         T/Itr(s)      T-Cumulative\n");
    printf("=====================================================\n");
    */

    //Start maximizing modularity
    while(true) {
        numItrs++;
        time1 = omp_get_wtime();
        /* Re-initialize datastructures */
        //#pragma omp parallel for
        for (long i=0; i<NV; i++) {
            clusterWeightInternal[i] = 0;
            cUpdate[i].degree =0;
            cUpdate[i].size =0;
        }

        //#pragma omp parallel for
        for (long i=0; i<NV; i++) {
            long adj1 = vtxPtr[i];
            long adj2 = vtxPtr[i+1];
            long selfLoop = 0;
            long size = NV;
            //Build a datastructure to hold the cluster structure of its neighbors
            dataItem** clusterLocalMap;
            //vector<double> Counter;
            //Number of edges to each unique cluster
            double* Counter;
            if (size != 0) {
                clusterLocalMap = (dataItem**)malloc(size * sizeof(dataItem*));
                for (long o = 0; o < size; o++) {
                    clusterLocalMap[o] = NULL;
                }
                Counter = (double*)malloc(size * sizeof(double));
                }
                //map<long, long>::iterator storedAlready;
                dataItem* temp = (dataItem*)malloc(sizeof(dataItem));
                if(adj1 != adj2) {
                    //Add v's current cluster:
                    insert(currCommAss[i], (long)0, size, clusterLocalMap);
                    Counter[0] = 0; //Initialize the counter to ZERO (no edges incident yet)
                    //Find unique cluster ids and #of edges incident (eicj) to them
                    selfLoop = buildLocalMapCounter(adj1, adj2, clusterLocalMap, Counter, vtxInd, currCommAss, i, NV);
                    // Update delta Q calculation
                    //printf("got %lf\n", Counter[0]);
                    //printf("clusterWeightInternal[i] %ld\n", clusterWeightInternal[i]);
                    clusterWeightInternal[i] += (long)Counter[0]; //(e_ix)
                    //printf("clusterWeightInternal[i], i : %ld, %ld\n", clusterWeightInternal[i], i);
                    //Calculate the max
                    targetCommAss[i] = max(clusterLocalMap, Counter, selfLoop, cInfo, vDegree[i], currCommAss[i], constantForSecondTerm, size);
                } else {
                    targetCommAss[i] = -1;
                }

                //Update
                if(targetCommAss[i] != currCommAss[i]  && targetCommAss[i] != -1) {
                    //printf("I am here uouo\n");
                    //printf("Inside here, targetCommAss[i] : %ld\n", targetCommAss[i]);
                    //printf("cUpdate[targetCommAss[i]].degree : %ld, vDegree[i] : %ld\n", cUpdate[targetCommAss[i]].degree, vDegree[i]);
                    //__sync_fetch_and_add(&cUpdate[targetCommAss[i]].degree, vDegree[i]);
                    cUpdate[targetCommAss[i]].degree += vDegree[i];
                    //printf("cUpdate[targetCommAss[i]].degree : %ld, vDegree[i] : %ld\n", cUpdate[targetCommAss[i]].degree, vDegree[i]);
                    //__sync_fetch_and_add(&cUpdate[targetCommAss[i]].size, 1);
                    cUpdate[targetCommAss[i]].size += 1;
                    //__sync_fetch_and_sub(&cUpdate[currCommAss[i]].degree, vDegree[i]);
                    cUpdate[currCommAss[i]].degree = cUpdate[currCommAss[i]].degree - vDegree[i];
                    //__sync_fetch_and_sub(&cUpdate[currCommAss[i]].size, 1);
                    cUpdate[currCommAss[i]].size = cUpdate[currCommAss[i]].size - 1;
                } //End of If()

                freeHashArr(clusterLocalMap, size);
                free(Counter);
        }

        time2 = omp_get_wtime();

        time3 = omp_get_wtime();

        double e_xx = 0;
        double a2_x = 0;
        //#pragma omp parallel for reduction(+:e_xx) reduction(+:a2_x)
        for (long i=0; i<NV; i++) {
            e_xx += (double)clusterWeightInternal[i];
            a2_x += (cInfo[i].degree)*(cInfo[i].degree);
        }
        time4 = omp_get_wtime();

        currMod = (e_xx*(double)constantForSecondTerm) - (a2_x*(double)constantForSecondTerm*(double)constantForSecondTerm);
        totItr = (time2-time1) + (time4-time3);

        total += totItr;
        printf("%d \t %g \t %g \t %lf \t %3.3lf \t %3.3lf  \t %3.3lf\n",numItrs, e_xx, a2_x, currMod, (time2-time1), (time4-time3), totItr );
        //printf("%d \t %lf \t %3.3lf  \t %3.3lf\n",numItrs, currMod, totItr, total);

        //Break if modularity gain is not sufficient
        if((currMod - prevMod) < thresMod) {
            break;
        }

        //Else update information for the next iteration
        prevMod = currMod;
        if(prevMod < Lower) {
            prevMod = Lower;
        }
        //#pragma omp parallel for
        for (long i=0; i<NV; i++) {
            //printf("cInfo[i].size , cInfo[i].degree : %ld, %ld\n", cInfo[i].size, cInfo[i].degree);
            cInfo[i].size += cUpdate[i].size;
            cInfo[i].degree += cUpdate[i].degree;
        }

        //Do pointer swaps to reuse memory:
        long* tmp;
        tmp = pastCommAss;
        pastCommAss = currCommAss; //Previous holds the current
        currCommAss = targetCommAss; //Current holds the chosen assignment
        targetCommAss = tmp;      //Reuse the vector

    } //End of while(true)

    *totTime = total; //Return back the total time for clustering
    *numItr  = numItrs;

    printf("========================================================================================================\n");
    printf("Total time for %d iterations is: %lf\n",numItrs, total);
    printf("========================================================================================================\n");


    //Store back the community assignments in the input variable:
    //Note: No matter when the while loop exits, we are interested in the previous assignment
    //#pragma omp parallel for
    for (long i=0; i<NV; i++) {
        C[i] = pastCommAss[i];
    }

    //Cleanup
    free(pastCommAss);
    free(currCommAss);
    free(targetCommAss);
    free(vDegree);
    free(cInfo);
    free(cUpdate);
    free(clusterWeightInternal);

    return prevMod;
}

double algoLouvainWithDistOneColoring(graph* G, long *C, int nThreads, int* color,
        int numColor, double Lower, double thresh, double *totTime, int *numItr) {
    printf("Within algoLouvainWithDistOneColoring()\n");
    /*
    if (nThreads < 1) {
        omp_set_num_threads(1);
    } else {
        omp_set_num_threads(nThreads);
    }
    */

    int nT;
    //#pragma omp parallel
    //{
        nT = omp_get_num_threads();
    //}
    printf("Actual number of threads: %d (requested: %d)\n", nT, nThreads);

    double time1, time2, time3, time4; //For timing purposes
    double total = 0, totItr = 0;
    /* Indexs are vertex */
    long* pastCommAss;      //Store previous iteration's community assignment
    long* currCommAss;      //Store current community assignment
    //long* targetCommAss;  //Store the target of community assignment
    long* vDegree;  //Store each vertex's degree
    long* clusterWeightInternal;//use for Modularity calculation (eii)

    /* Indexs are community */
    comm* cInfo;     //Community info. (ai and size)
    comm* cUpdate; //use for updating Community

    /* Book keeping variables */
    long    NV        = G->numVertices;
    long    NS        = G->sVertices;
    long    NE        = G->numEdges;
    long    *vtxPtr   = G->edgeListPtrs;
    edge    *vtxInd   = G->edgeList;

    /* Modularity Needed variables */
    long totalEdgeWeightTwice;
    double constantForSecondTerm;
    double prevMod=Lower;
    double currMod=-1;
    double thresMod = thresh;
    int numItrs = 0;

    /********************** Initialization **************************/
    time1 = omp_get_wtime();
    vDegree = (long *) malloc (NV * sizeof(long)); 
    assert(vDegree != 0);
    cInfo = (comm *) malloc (NV * sizeof(comm)); 
    assert(cInfo != 0);
    cUpdate = (comm*)malloc(NV*sizeof(comm)); 
    assert(cUpdate != 0);

    sumVertexDegree(vtxInd, vtxPtr, vDegree, NV , cInfo);   // Sum up the vertex degree
    /*** Compute the total edge weight (2m) and 1/2m ***/
    constantForSecondTerm = calConstantForSecondTerm(vDegree, NV);  // 1 over sum of the degree

    pastCommAss = (long *) malloc (NV * sizeof(long)); 
    assert(pastCommAss != 0);
    //Community provided as input:
    currCommAss = C; 
    assert(currCommAss != 0);
    
    /*** Assign each vertex to its own Community ***/
    initCommAss( pastCommAss, currCommAss, NV);

    clusterWeightInternal = (long*) malloc (NV*sizeof(long)); 
    assert(clusterWeightInternal != 0);

    /*** Create a CSR-like datastructure for vertex-colors ***/
    long * colorPtr = (long *) malloc ((numColor+1) * sizeof(long));
    long * colorIndex = (long *) malloc (NV * sizeof(long));
    long * colorAdded = (long *)malloc (numColor*sizeof(long));
    assert(colorPtr != 0);
    assert(colorIndex != 0);
    assert(colorAdded != 0);
    // Initialization
    //#pragma omp parallel for
    for(long i = 0; i < numColor; i++) { 
        colorPtr[i] = 0;
        colorAdded[i] = 0;
    }
    colorPtr[numColor] = 0;
    // Count the size of each color
    //#pragma omp parallel for
    for(long i = 0; i < NV; i++) {
        //__sync_fetch_and_add(&colorPtr[(long)color[i]+1],1);
        colorPtr[(long)color[i]+1]++;
    }
    //Prefix sum:
    for(long i=0; i<numColor; i++) {
        colorPtr[i+1] += colorPtr[i];
    }
    //Group vertices with the same color in particular order
    //#pragma omp parallel for
    for (long i=0; i<NV; i++) {
        long tc = (long)color[i];
        long Where = colorPtr[tc] + colorAdded[tc]++; 
        //long Where = colorPtr[tc] + __sync_fetch_and_add(&(colorAdded[tc]), 1);
        colorIndex[Where] = i;
    }
    time2 = omp_get_wtime();
    printf("Time to initialize: %3.3lf\n", (time2-time1));

    printf("========================================================================================================\n");
    printf("Itr      E_xx            A_x2           Curr-Mod         Time-1(s)       Time-2(s)        T/Itr(s)\n");
    printf("========================================================================================================\n");

    /*
    printf("=====================================================\n");
    printf("Itr      Curr-Mod         T/Itr(s)      T-Cumulative\n");
    printf("=====================================================\n");
    */

    while(true) {
        numItrs++;
        time1 = omp_get_wtime();
        for( long ci = 0; ci < numColor; ci++) {// Begin of color loop
            //#pragma omp parallel for
            for (long i=0; i<NV; i++) {
                //printf("processing i = %ld\n", i);
                clusterWeightInternal[i] = 0; //Initialize to zero
                cUpdate[i].degree =0;
                cUpdate[i].size =0;
            }
            long coloradj1 = colorPtr[ci];
            long coloradj2 = colorPtr[ci+1];

            //#pragma omp parallel for
            for (long K = coloradj1; K<coloradj2; K++) {
                long i = colorIndex[K];
                long localTarget = -1;
                long adj1 = vtxPtr[i];
                long adj2 = vtxPtr[i+1];
                long selfLoop = 0;
                //Build a datastructure to hold the cluster structure of its neighbors:
                // map each neighbor's cluster to a local number
                //long size = adj2 - adj1;
                long size = NV;
                //printf("size is %ld\n", size);
                //map<long, long> clusterLocalMap; //Map each neighbor's cluster to a local number
                dataItem** clusterLocalMap;
                //vector<double> Counter;
                //Number of edges to each unique cluster
                double* Counter;
                if (size != 0) {
                    clusterLocalMap = (dataItem**)malloc(size * sizeof(dataItem*));
                    for (long i = 0; i < size; i++) {
                        clusterLocalMap[i] = NULL;
                        //printf("true\n");
                    }
                    Counter = (double*)malloc(size * sizeof(double));
                }
                //map<long, long>::iterator storedAlready;
                dataItem* temp = (dataItem*)malloc(sizeof(dataItem));

                if(adj1 != adj2) {
                    //Add v's current cluster:
                    //printf("Inside here\n");
                    //printf("currCommAss[i] : %ld\n", currCommAss[i]);
                    insert(currCommAss[i], (long)0, size, clusterLocalMap);
                    //temp = search(currCommAss[i], size, clusterLocalMap);
                    //printf("temp->data : %ld\n", temp->data);
                    Counter[0] = 0; //Initialize the counter to ZERO (no edges incident yet)
                    //Find unique cluster ids and #of edges incident (eicj) to them
                    selfLoop = buildLocalMapCounter(adj1, adj2, clusterLocalMap, Counter, vtxInd, currCommAss, i, NV);
                    //Calculate the max
                    localTarget = max(clusterLocalMap, Counter, selfLoop, cInfo, vDegree[i], currCommAss[i], constantForSecondTerm, size);
                } else {
                    localTarget = -1;
                }
                //Update prepare
                if(localTarget != currCommAss[i] && localTarget != -1) {
                    //__sync_fetch_and_add(&cUpdate[localTarget].degree, vDegree[i]);
                    cUpdate[localTarget].degree += vDegree[i];
                    //__sync_fetch_and_add(&cUpdate[localTarget].size, 1);
                    cUpdate[localTarget].size += 1;
                    //__sync_fetch_and_sub(&cUpdate[currCommAss[i]].degree, vDegree[i]);
                    cUpdate[currCommAss[i]].degree = cUpdate[currCommAss[i]].degree - vDegree[i];
                    //__sync_fetch_and_sub(&cUpdate[currCommAss[i]].size, 1);
                    cUpdate[currCommAss[i]].size = cUpdate[currCommAss[i]].size - 1;
                } // End of if
                currCommAss[i] = localTarget;
                freeHashArr(clusterLocalMap, size);
                free(Counter);
            } // End of for(i)
            // UPDATE
            // #pragma omp parallel for
            //printf("I am here\n");
            for (long i=0; i<NV; i++) {
                cInfo[i].size += cUpdate[i].size;
                cInfo[i].degree += cUpdate[i].degree;
            }
        } // End of Color loop
        time2 = omp_get_wtime();

        time3 = omp_get_wtime();
        double e_xx = 0;
        double a2_x = 0;
        // CALCULATE MOD
        // #pragma omp parallel for  //Parallelize on each vertex
        for (long i =0; i<NV;i++){
            clusterWeightInternal[i] = 0;
        }
        // #pragma omp parallel for  //Parallelize on each vertex
        for (long i=0; i<NV; i++) {
            long adj1 = vtxPtr[i];
            long adj2 = vtxPtr[i+1];
            for(long j=adj1; j<adj2; j++) {
                if(currCommAss[vtxInd[j].tail] == currCommAss[i]){
                    clusterWeightInternal[i] += (long)vtxInd[j].weight;
                }
            }
        }
        // #pragma omp parallel for reduction(+:e_xx) reduction(+:a2_x)
        for (long i=0; i<NV; i++) {
            e_xx += clusterWeightInternal[i];
            a2_x += (cInfo[i].degree)*(cInfo[i].degree);
        }
        time4 = omp_get_wtime();
        currMod = e_xx*(double)constantForSecondTerm  - a2_x*(double)constantForSecondTerm*(double)constantForSecondTerm;

        totItr = (time2-time1) + (time4-time3);
        total += totItr;
        printf("%d \t %g \t %g \t %lf \t %3.3lf \t %3.3lf  \t %3.3lf\n",numItrs, e_xx, a2_x, currMod, (time2-time1), (time4-time3), totItr );

        printf("%d \t %lf \t %3.3lf  \t %3.3lf\n",numItrs, currMod, totItr, total);
        if((currMod - prevMod) < thresMod) {
            break;
        }
        prevMod = currMod;
    } //End of while(true)
    *totTime = total; //Return back the total time
    *numItr  = numItrs;
    
    printf("========================================================================================================\n");
    printf("Total time for %d iterations is: %lf\n",numItrs, total);
    printf("========================================================================================================\n");

    printf("========================================================================================================\n");
    printf("Total time for %d iterations is: %lf\n",numItrs, total);
    printf("========================================================================================================\n");

    //Cleanup:
    free(vDegree);
    free(cInfo);
    free(cUpdate);
    free(clusterWeightInternal);
    free(colorPtr);
    free(colorIndex);
    free(colorAdded);
    free(pastCommAss);

    return prevMod;
} //End of algoLouvainWithDistOneColoring()


// WARNING: Will assume that the cluster id have been renumbered contiguously
// Return the total time for building the next level of graph
double buildNextLevelGraphOpt(graph *Gin, graph *Gout, long *C, long numUniqueClusters, int nThreads) {
    printf("Within buildNextLevelGraphOpt(): # of unique clusters= %ld\n",numUniqueClusters);
    /*
    if (nThreads < 1) {
        omp_set_num_threads(1);
    } else {
        omp_set_num_threads(nThreads);
    }
    */
    //int nT;
    //#pragma omp parallel
    //{
    //  nT = omp_get_num_threads();
    //}

    //printf("Actual number of threads: %d (requested: %d)\n", nT, nThreads);
    double time1, time2;
    double TotTime=0; //For timing purposes
    double total = 0, totItr = 0;
    //Pointers into the input graph structure:
    long    NV_in        = Gin->numVertices;
    long    NE_in        = Gin->numEdges;
    long    *vtxPtrIn    = Gin->edgeListPtrs;
    edge    *vtxIndIn    = Gin->edgeList;

    time1 = omp_get_wtime();
    // Pointers into the output graph structure
    long NV_out = numUniqueClusters;
    long NE_out = 0;
    long *vtxPtrOut = (long *) malloc ((NV_out+1)*sizeof(long)); 
    assert(vtxPtrOut != 0);
    vtxPtrOut[0] = 0; //First location is always a zero
    /* Step 1 : Regroup the node into cluster node */
    printf("numUniqueClusters : %ld\n", numUniqueClusters);
    printf("NV_in : %ld\n", NV_in);
    dataItem*** cluPtrIn = (dataItem***)malloc(numUniqueClusters * sizeof(dataItem**));
    assert(cluPtrIn != 0);
    long* sizeArr = (long*)malloc(numUniqueClusters * sizeof(long));
    assert(sizeArr != 0);
    
    //#pragma omp parallel for
    // keep track of zero weight
    /*
    for (long i=0; i<numUniqueClusters; i++) {
        cluPtrIn[i] = new map<long,long>();
        (*(cluPtrIn[i]))[i] = 0; //Add for a self loop with zero weight
    }
    */
    for (long i=0; i<numUniqueClusters; i++) {
        cluPtrIn[i] = (dataItem**)malloc(numUniqueClusters * (sizeof(dataItem*)));
        for (long k = 0; k < numUniqueClusters; k++) {
            cluPtrIn[i][k] = NULL;
        }
        insert(i, (long)0, numUniqueClusters, cluPtrIn[i]); // Add for a self loop with zero weight 
        //displayHashMap(cluPtrIn[i], numUniqueClusters);
        //printf("key : %ld\n", cluPtrIn[i][i]->key);
        //printf("value : %ld\n", cluPtrIn[i][i]->data);
    }
    //printf("I am here\n");


    //#pragma omp parallel for
    for (long i = 1; i <= NV_out; i++) {
        vtxPtrOut[i] = 1; // Count self-loops for every vertex
    }

    //Create an array of locks for each cluster
    /*
    omp_lock_t *nlocks = (omp_lock_t *) malloc (numUniqueClusters * sizeof(omp_lock_t));
    assert(nlocks != 0);
    #pragma omp parallel for
    for (long i=0; i<numUniqueClusters; i++) {
        omp_init_lock(&nlocks[i]); //Initialize locks
    }
    */
    time2 = omp_get_wtime();
    TotTime += (time2-time1);

    printf("Time to initialize: %3.3lf\n", (time2-time1));

    time1 = omp_get_wtime();

    /*
    //#pragma omp parallel for
    for (long i=0; i<NV_in; i++) {
        printf("i : %ld\n", i);
        long adj1 = vtxPtrIn[i];
        long adj2 = vtxPtrIn[i+1];
        assert(C[i] < numUniqueClusters);
        //Now look for all the neighbors of this cluster
        for(long j=adj1; j<adj2; j++) {
            long tail = vtxIndIn[j].tail;
            assert(C[tail] < numUniqueClusters);
            //Add the edge from one endpoint
            if(C[i] > C[tail]) {
                sizeArr[C[i]]++;
            } 
            if (C[i] == C[tail] && i <= tail) {
                sizeArr[C[i]]++;
            }
        }
        printf("here\n");
        cluPtrIn[C[i]] = (dataItem**)malloc(sizeArr[C[i]] * (sizeof(dataItem)));
        for (long k = 0; k < sizeArr[C[i]]; k++) {
            cluPtrIn[C[i]][k] = NULL;
            printf("true\n");
        }
    }
    */
    /*
    for (long i = 0; i < numUniqueClusters; i++) {
        displayHashMap(cluPtrIn[C[i]], numUniqueClusters);
    }
    */

    for (long i = 0; i < NV_in; i++) {
        long adj1 = vtxPtrIn[i];
        long adj2 = vtxPtrIn[i+1];
        dataItem* temp = (dataItem*)malloc(sizeof(dataItem));
        assert(temp != 0);
        assert(C[i] < numUniqueClusters);
        // Now look for all the neighbors of this cluster
        for (long j = adj1; j < adj2; j++) {
            long tail = vtxIndIn[j].tail;
            assert(C[tail] < numUniqueClusters);
            //Add the edge from one endpoint
            if(C[i] >= C[tail]) {
            // Care about this later
            /*
                omp_set_lock(&nlocks[C[i]]);  // Locking the cluster
            */
                temp = search(C[tail], numUniqueClusters, cluPtrIn[C[i]]);
                //displayHashMap(cluPtrIn[3], numUniqueClusters);
                
                if (temp != NULL) {
                    //printf("C[tail] is %ld and i is %ld and C[i] is %ld\n", C[tail], i, C[i]);
                    //displayHashMap(cluPtrIn[C[i]], numUniqueClusters);
                    //printf("adding %ld\n", (long)vtxIndIn[j].weight);
                    temp->data += (long)vtxIndIn[j].weight;
                    //printf("new temp->data %ld\n", temp->data);
                    //displayHashMap(cluPtrIn[C[i]], numUniqueClusters);
                } else {
                    //printf("I am inserting : %ld\n", (long)vtxIndIn[j].weight);
                    //printf("C[i] is %ld\n", C[i]);
                    //displayHashMap(cluPtrIn[C[i]], numUniqueClusters);
                    insert(C[tail], (long)vtxIndIn[j].weight, numUniqueClusters, cluPtrIn[C[i]]); // Inter-community edge
                    //printf("C[i] is %ld\n", C[i]);
                    //displayHashMap(cluPtrIn[C[i]], numUniqueClusters);
                    //__sync_fetch_and_add(&vtxPtrOut[C[i]+1], 1);
                    vtxPtrOut[C[i]+1] = vtxPtrOut[C[i]+1] + 1;
                    if(C[i] > C[tail]) {
                        //Keep track of non-self #edges
                        //__sync_fetch_and_add(&NE_out, 1);
                        NE_out++;
                        //Count edge j-->i
                        //__sync_fetch_and_add(&vtxPtrOut[C[tail]+1], 1);
                        vtxPtrOut[C[tail]+1] = vtxPtrOut[C[tail]+1] + 1;
                    }
                }
                //printf("illa2\n");
                //displayHashMap(cluPtrIn[3], numUniqueClusters);
                /* Dont understand the point of this now
                omp_unset_lock(&nlocks[C[i]]); // Unlocking the cluster
                */
            } // End of if
        } // End of for(j)
        //free(temp);
    }// End of for(i)
    //Prefix sum:
    for(long i=0; i<NV_out; i++) {
        vtxPtrOut[i+1] += vtxPtrOut[i];
    }

    time2 = omp_get_wtime();
    TotTime += (time2 - time1);
    printf("NE_out : %ld\n", NE_out);
    printf("NV_out : %ld\n", NV_out);
    printf("These should match: %ld == %ld\n",(2*NE_out + NV_out), vtxPtrOut[NV_out]);
    printf("Time to count edges: %3.3lf\n", (time2 - time1));
    assert(vtxPtrOut[NV_out] == (NE_out*2+NV_out)); //Sanity check

    time1 = omp_get_wtime();
    // Step 3 : build the edge list:
    long numEdges   = vtxPtrOut[NV_out];
    long realEdges  = numEdges - NE_out; //Self-loops appear once, others appear twice
    edge *vtxIndOut = (edge *)malloc(numEdges * sizeof(edge));
    assert (vtxIndOut != 0);
    long *Added = (long *) malloc (NV_out * sizeof(long)); //Keep track of what got added
    assert (Added != 0);

    //#pragma omp parallel for
    for (long i=0; i<NV_out; i++) {
        Added[i] = 0;
    }

    /*
    for (long i = 0; i < NV_out; i++) {
        displayHashMap(cluPtrIn[i], NV_out);
        for (long s = 0; s < NV_out; s++) {
            if (!cluPtrIn[i][s]) {
                printf("NULL i : %ld s : %ld\n", i, s);
            } else {
                printf("Non NULL i : %ld s : %ld\n", i, s);
                printf("key : %ld\n", cluPtrIn[i][s]->key);
                printf("value : %ld\n", cluPtrIn[i][s]->data);
            }
        }
    } 
    */

    //Now add the edges in no particular order
    //#pragma omp parallel for
    dataItem* temp = (dataItem*)malloc(sizeof(dataItem));
    for (long i = 0; i < NV_out; i++) {
        //printf("step 3 : i : %ld\n", i);
        long j = 0;
        long Where;
        temp = cluPtrIn[i][j];
        // Now go through the other edges
        while (j < numUniqueClusters) {
            // Don't understand the point of this right now
            // Where = vtxPtrOut[i] + __sync_fetch_and_add(&Added[i], 1);
            //printf("I am here\n");
            //printf("j : %ld\n", j);
            if (temp != NULL) {
            Where = vtxPtrOut[i] + Added[i]++;
            //printf("Where : %ld\n", Where);
            vtxIndOut[Where].head = i; // Head
            //printf("i : %ld\n", i);
            vtxIndOut[Where].tail = temp->key; // Tail
            //printf("temp->key : %ld\n", temp->key);
            vtxIndOut[Where].weight = temp->data; // Weight
            //printf("temp->data : %ld\n", temp->data);
            if (i != temp->key) {
                // Don't understand the point of this now
                // Where = vtxPtrOut[temp->key] + __sync_fetch_and_add(&Added[i], 1);
                //
                //printf("i != temp->key\n");
                Where = vtxPtrOut[temp->key] + Added[temp->key]++;
                //printf("I am here\n");
                vtxIndOut[Where].head = temp->key;
                vtxIndOut[Where].tail = i; // Tail
                vtxIndOut[Where].weight = temp->data; // Weight
            }
            }
            j++;
            temp = cluPtrIn[i][j];
        } // End of while
    } // End of for(i)
    //free(temp);

    time2 = omp_get_wtime();
    TotTime += (time2-time1);
    printf("Time to build graph: %3.3lf\n", (double)(time2 - time1)/CLOCKS_PER_SEC);
    printf("Total time: %3.3lf\n", TotTime);
    printf("Total time to build next phase: %3.3lf\n", TotTime);

    // Set the pointers
    Gout->numVertices  = NV_out;
    Gout->sVertices    = NV_out;
    //Note: Self-loops are represented ONCE, but others appear TWICE
    Gout->numEdges     = realEdges; //Add self loops to the #edges
    Gout->edgeListPtrs = vtxPtrOut;
    Gout->edgeList     = vtxIndOut;

    //Clean up
    free(Added);

    // Don't understand the point of it right now
    //#pragma omp parallel for
    for (long i=0; i<numUniqueClusters; i++) {
        freeHashArr(cluPtrIn[i], numUniqueClusters);    
    }
    free(cluPtrIn);
            //printf("I am here\n");

    //#pragma omp parallel for
    /*
    for (long i=0; i<numUniqueClusters; i++) {
        omp_destroy_lock(&nlocks[i]);
    }
    free(nlocks);
    */
    return TotTime;
} // End of buildNextLevelGraphOpt


// function : runMultiPhaseLouvainAlgorithm
// WARNING : This will overwrite the original graph data structure to
// minimize memory footprint
// Return : C_orig will hold the cluster ids for vertices in the 
// original graph. Assume C_orig is initialized appropriately
// WARNING : Graph G will be destroyed at the end of this routine.
void runMultiPhaseLouvainAlgorithm(graph* G, long* C_orig, int coloring, long minGraphSize, 
        double threshold, double C_threshold, int numThreads) {
    double totTimeClustering=0, totTimeBuildingPhase=0, totTimeColoring=0, tmpTime;
    int tmpItr=0, totItr=0;
    long NV = G->numVertices;

    // long minGraphSize = 100000; // Need atleast 100,000 vertices to turn coloring on

    int* colors;
    int numColors;
    if (coloring == 1) {
        colors = (int*)malloc(G->numVertices * sizeof(int));
        assert(colors != 0);
        // Don't know what to do with this right now
        //#pragma omp parallel for
        for (long i = 0; i < G->numVertices; i++) {
            colors[i] = -1;
        }
        numColors = algoDistanceOneVertexColoringOpt(G, colors, numThreads, &tmpTime) + 1;
        totTimeColoring += tmpTime;
        printf("Number of colors used : %d\n", numColors);
    }

    /* Step 3: Find communities */
    double prevMod = -1;
    double currMod = -1;
    long phase = 1;

    graph *Gnew; //To build new hierarchical graphs
    long numClusters;
    long *C = (long *) malloc (NV * sizeof(long));
    assert(C != 0);
    
    // #pragma omp parallel for
    for (long i=0; i<NV; i++) {
        C[i] = -1;
    }
    bool nonColor = false; //Make sure that at least one phase with lower threshold runs
    while(1) {
        printf("===============================\n");
        printf("Phase %ld\n", phase);
        printf("===============================\n");
        prevMod = currMod;
        //Compute clusters
        if((coloring == 1)&&(G->numVertices > minGraphSize)&&(nonColor == false)) {
            //Use higher modularity for the first few iterations when graph is big enough
            currMod = algoLouvainWithDistOneColoring(G, C, numThreads, colors, numColors, currMod, C_threshold, &tmpTime, &tmpItr);
            totTimeClustering += tmpTime;
            totItr += tmpItr;
        } else {
            currMod = parallelLouvianMethod(G, C, numThreads, currMod, threshold, &tmpTime, &tmpItr);
            totTimeClustering += tmpTime;
            totItr += tmpItr;
            nonColor = true;
        }
        //Renumber the clusters contiguiously
        numClusters = renumberClustersContiguously(C, G->numVertices);
        printf("Number of unique clusters: %ld\n", numClusters);
        //printf("About to update C_orig\n");
        //Keep track of clusters in C_orig
        if(phase == 1) {
        //#pragma omp parallel for
            for (long i=0; i<NV; i++) {
                C_orig[i] = C[i]; //After the first phase
            }
        } else {
        //#pragma omp parallel for
            for (long i=0; i<NV; i++) {
                assert(C_orig[i] < G->numVertices);
                if (C_orig[i] >=0) {
                    C_orig[i] = C[C_orig[i]]; //Each cluster in a previous phase becomes a vertex
                }
            }
        }
        printf("Done updating C_orig\n");
        //Break if too many phases or iterations
        if((phase > 200)||(totItr > 10000)) {
            break;
        }
        //Check for modularity gain and build the graph for next phase
        //In case coloring is used, make sure the non-coloring routine is run at least once
        if( (currMod - prevMod) > threshold ) {
            Gnew = (graph *) malloc (sizeof(graph)); 
            assert(Gnew != 0);
            tmpTime = buildNextLevelGraphOpt(G, Gnew, C, numClusters, numThreads);
            totTimeBuildingPhase += tmpTime;
            //Free up the previous graph
            free(G->edgeListPtrs);
            free(G->edgeList);
            free(G);
            G = Gnew; //Swap the pointers
            G->edgeListPtrs = Gnew->edgeListPtrs;
            G->edgeList = Gnew->edgeList;
            //Free up the previous cluster & create new one of a different size
            free(C);
            C = (long *) malloc (numClusters * sizeof(long)); 
            assert(C != 0);
            //#pragma omp parallel for
            for (long i=0; i<numClusters; i++) {
                C[i] = -1;
            }
            phase++; //Increment phase number
            //If coloring is enabled & graph is of minimum size, recolor the new graph
            if((coloring == 1)&&(G->numVertices > minGraphSize)&&(nonColor = false)){
                //#pragma omp parallel for
                for (long i=0; i<G->numVertices; i++){
                    colors[i] = -1;
                }
                numColors = algoDistanceOneVertexColoringOpt(G, colors, numThreads, &tmpTime)+1;
                totTimeColoring += tmpTime;
            }
        } else {
            if ( (coloring == 1)&&(nonColor == false) ) {
                nonColor = true; //Run at least one loop of non-coloring routine
            } else {
                break; //Modularity gain is not enough. Exit.
            }
        }
    } //End of while(1)

    printf("********************************************\n");
    printf("*********    Compact Summary   *************\n");
    printf("********************************************\n");
    printf("Number of threads              : %ld\n", numThreads);
    printf("Total number of phases         : %ld\n", phase);
    printf("Total number of iterations     : %ld\n", totItr);
    printf("Final number of clusters       : %ld\n", numClusters);
    printf("Final modularity               : %lf\n", prevMod);
    printf("Total time for clustering      : %lf\n", totTimeClustering);
    printf("Total time for building phases : %lf\n", totTimeBuildingPhase);
    if(coloring == 1) {
        printf("Total time for coloring        : %lf\n", totTimeColoring);
    }
    printf("********************************************\n");
    printf("TOTAL TIME                     : %lf\n", (totTimeClustering+totTimeBuildingPhase+totTimeColoring) );
    printf("********************************************\n");

    //Clean up:
    free(C);
    if(G != 0) {
        free(G->edgeListPtrs);
        free(G->edgeList);
        free(G);
    }

    if(coloring==1) {
        if(colors != 0) free(colors);
    }
} //End of runMultiPhaseLouvainAlgorithm()

// function : main
int main(int argc, char** argv) {
    // Step1 : Parse Input Parameters
    //utilityParseInputParameters.hpp
    //printf("Inside main\n");
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

    int nT = 1; // default number of threads is 1
    // Check for number of threads
    // Dont understand the point of it now
    /*
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

double start, end;
double cpu_time_used;
// Vertex Following option
if (inputParams->VF) {
    printf("Vertex following is enabled.\n");
    start = omp_get_wtime();
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
        buildNewGraphVF(G, Gnew, C, numClusters);
        free(G->edgeListPtrs);
        free(G->edgeList);
        free(G);
        G = Gnew;
    }
    free(C); // Free up memory
    printf("Graph after modifications:\n");
    displayGraphCharacteristics(G);
} // End of if (VF == 1)

// Datastructures to store clustering information
long NV = G->numVertices;
long* C_orig = (long*)malloc(NV * sizeof(long));

// Call the clustering algorithm
// They call strong scaling - I don't know what to do
// with it right now
// No strong scaling
//#pragma omp parallel for
for (int i = 0; i < NV; i++) {
    C_orig[i] = -1;
}
runMultiPhaseLouvainAlgorithm(G, C_orig, coloring, inputParams->minGraphSize, inputParams->threshold, inputParams->C_thresh, nT);

//Check if cluster ids need to be written to a file:
if( inputParams->output ) {
    char outFile[256];
    sprintf(outFile,"%s_clustInfo", inputParams->inFile);
    printf("Cluster information will be stored in file: %s\n", outFile);
    FILE* out = fopen(outFile,"w");
    for(long i = 0; i<NV;i++) {
        fprintf(out,"%ld\n",C_orig[i]);
    }
    fclose(out);
}

//Cleanup:
if(C_orig != 0) free(C_orig);


// Free the memory space allocated for struct clusteringParams
free(inputParams);
// Free the memory space allocated for struct graph
free(G);

return 0;
}
