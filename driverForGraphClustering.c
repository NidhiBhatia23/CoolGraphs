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

// function : renumberClustersContiguously
// WARNING : will overwrite the old cluster
// Returns the number of unique clusters
long renumberClustersContiguously(long* C, long size) {
    printf("Inside renumberClustersContiguously\n");
}


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
