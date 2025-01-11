#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
using namespace std;
#include <set>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <map>
#include <set>
#include <algorithm>
#include <limits>
#include <sstream>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100
#define INIT_MSG 29
#define START_TAG 20
#define REQUEST_TAG 101
#define SWARM_TAG 102
#define SEGREQ_TAG 103
#define SEGRESP_TAG 104
#define FINDOWN_TAG 105
#define FINAL_TAG 106
#define TURNOFF_TAG 107
static pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;
struct TrackerInfo{
    vector<int> seeds;
    vector<int> peers;
    vector<string> segments;
    int numS;
    string filename;
};
struct File {
    string filename;
    int numSegm;
    vector<string> segments;
    bool downloaded;
};;
struct WantedFIle{
    string name;
    bool downloaded;
};
struct ClientsInfo{
    vector<File> owned;
    vector<File> downloaded;
    vector<WantedFIle> wantedfiles;
    map<string,int> liveupdate;
};
vector <ClientsInfo> clients;
map<string,TrackerInfo> trackerData;
int getDownloadedFile(const std::string &filename, int rank)
{
    // caut fisierul in lista daca nu il am creez si il adaug
    for (int i = 0; i < (int) clients[rank].downloaded.size(); i++) {
        if (clients[rank].downloaded[i].filename == filename) {
            return i;
        }
    }
    File newFile;
    newFile.filename = filename;
    newFile.numSegm  = 0;
    newFile.downloaded = false;
    clients[rank].downloaded.push_back(newFile);

    int newIndex = clients[rank].downloaded.size() - 1;
    return newIndex;
}
void storeInput(int rank){
    //prelucrez fisierele de input
    string string_rank = to_string(rank);
    string filename = "in" + string_rank + ".txt";
    string filepath = "../checker/tests/test1/" + filename;
    std::ifstream in(filename);
    if (!in.is_open())
            std::cerr << "Error: Could not open file " << filename << "\n";
    if((int)clients.size() < rank)
        clients.resize(rank + 1);
    int nr;
    in >> nr;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    while(nr){
        string name;
        int nr_segments;
        getline(in,name,' ');
        in >> nr_segments;
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        File new_file;
        new_file.downloaded = true;
        new_file.filename = name;
        new_file.numSegm = nr_segments;
        string hash;
        while(nr_segments){
            in >> hash;
            new_file.segments.push_back(hash);
            nr_segments--;
        }
        in.ignore();
        nr--;
        clients[rank].owned.push_back(new_file); 
    }
    int nr_wanted;
    in >> nr_wanted;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    string name;
    while(nr_wanted){
        WantedFIle wanted;
        in >> name;
        wanted.name = name;
        wanted.downloaded = false;
        nr_wanted--;
        clients[rank].wantedfiles.push_back(wanted);
    }
}
void sendtotracker(int rank){
    string message;
    int nr = clients[rank].owned.size();
    // trimit fisierele pe care le detine sub forma unui string  
    message = to_string(nr) + " ";
    for(int i = 0 ; i < nr; i ++){
        message += clients[rank].owned[i].filename + " ";
        int nrseg = clients[rank].owned[i].numSegm;
        message += to_string(nrseg) + " ";
        for(int j = 0; j < nrseg; j++)
            message += clients[rank].owned[i].segments[j] + " ";    
    }
    MPI_Send(message.data(), message.size(), MPI_CHAR, TRACKER_RANK, INIT_MSG, MPI_COMM_WORLD);
}
void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;
    storeInput(rank);
    sendtotracker(rank);
    // trimit datele la tracker si astept raspunsul pentru a incepe
    MPI_Status status;
    MPI_Probe(TRACKER_RANK, START_TAG, MPI_COMM_WORLD, &status);
    int size;
    MPI_Get_count(&status, MPI_CHAR ,&size);
    int src = status.MPI_SOURCE;
    int tag = status.MPI_TAG;
    string mes;
    mes.resize(size);
    MPI_Recv(&mes[0],size,MPI_CHAR,src,tag,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    srand((unsigned)time(NULL) + rank);
    if(mes.compare("begin") !=0)
        exit(0);
    for(auto& x : clients[rank].wantedfiles){ 
        string filename = x.name;
        //pentru fiecare fisier cer swarm-ul de la tracker
        MPI_Send(filename.data(), filename.size(), MPI_CHAR, TRACKER_RANK, REQUEST_TAG, MPI_COMM_WORLD);

        MPI_Status status;
        MPI_Probe(TRACKER_RANK, SWARM_TAG, MPI_COMM_WORLD, &status);
        int size;
        MPI_Get_count(&status, MPI_CHAR ,&size);
        int src = status.MPI_SOURCE;
        int tag = status.MPI_TAG;
        string mes;
        mes.resize(size);
        MPI_Recv(&mes[0],size,MPI_CHAR,src,tag,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
        std::istringstream iss(mes);
        int nr_swarms;
        iss >> nr_swarms;
        std::vector<int> swarms(nr_swarms);
        for(int i = 0; i < nr_swarms; ++i) {
            iss >> swarms[i];
        }
        int nr_seg;
        iss >> nr_seg;
        std::vector<std::string> segments(nr_seg);
        for(int i = 0; i < nr_seg; ++i) {
            iss >> segments[i];
        }

        int fileIndex = getDownloadedFile(x.name, rank);

        int segmentsToDownload, segindex;
        int numSegm = clients[rank].downloaded[fileIndex].numSegm;
        if(numSegm == 0){
            segmentsToDownload = nr_seg;
            segindex = 0;
        }
        else{
            segmentsToDownload = nr_seg - numSegm;
            segindex = numSegm;     
        }
        int ct = 0;
        while(segmentsToDownload && segindex < nr_seg){
            if(ct == 10){
                //cer swarm actualizat odata la 10 downloadari
                MPI_Status status1;
                MPI_Send(filename.data(), filename.size(), MPI_CHAR, TRACKER_RANK, REQUEST_TAG, MPI_COMM_WORLD);
                MPI_Probe(TRACKER_RANK, SWARM_TAG, MPI_COMM_WORLD, &status1);
                MPI_Get_count(&status1, MPI_CHAR ,&size);
                src = status1.MPI_SOURCE;
                tag = status1.MPI_TAG;
                string mes;
                mes.resize(size);
                MPI_Recv(&mes[0],size,MPI_CHAR,src,tag,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
                std::istringstream iss(mes);
                iss >> nr_swarms;
                for(int i = 0; i < nr_swarms; ++i) {
                    iss >> swarms[i];
                }
                ct = 0;
                continue;
            }
            int randomPeer = swarms[rand() % swarms.size()];
            //cer segmentul
            string req_mes;
            req_mes += filename +" "+  to_string(segindex) + " " + segments[segindex];
            MPI_Send(req_mes.data(),req_mes.size(),MPI_CHAR,randomPeer,SEGREQ_TAG,MPI_COMM_WORLD);
            MPI_Status status;
            MPI_Probe(randomPeer,SEGRESP_TAG , MPI_COMM_WORLD, &status);
            int size;
            MPI_Get_count(&status, MPI_CHAR ,&size);
            int src = status.MPI_SOURCE;
            int tag = status.MPI_TAG;
            string mes;
            mes.resize(size);
            //daca este ok si am confirmarea segmentului adaug id-ul segmentului
            //in map ul liveupdate
            MPI_Recv(&mes[0],size,MPI_CHAR,src,tag,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
            if(mes == "OK"){
                clients[rank].liveupdate[filename] = segindex;
                segmentsToDownload--;
                segindex++;
                ct++;
            }
            else
                ct++;
        }
        //la final adaug fisierul in lista de donwloadate 
        File file = clients[rank].downloaded[fileIndex];
        file.numSegm = nr_seg;
        file.downloaded = true;
        for(int i = 0; i < nr_seg; i++)
            file.segments.push_back(segments[i]);
        x.downloaded = true;
        string msg = filename;
        // anunt tracker ul ca am terminat de descarcat fisierul 
        MPI_Send(msg.data(), msg.size(), MPI_CHAR, TRACKER_RANK, FINDOWN_TAG, MPI_COMM_WORLD);
        string output_name = "client" + to_string(rank) + "_" + filename;
        ofstream out(output_name);
        for(int i = 0 ; i < nr_seg; i++)
            out << file.segments[i] << endl;
        out.close();
    }
    string msg = "FINALIZED";
    MPI_Send(msg.data(), msg.size(), MPI_CHAR, TRACKER_RANK, FINAL_TAG, MPI_COMM_WORLD); 
    pthread_exit(NULL);
    return NULL;
}
bool haveSegm(string filename,int segindex,string hash,int rank){
    vector<File> downloaded = clients[rank].downloaded;
    vector<File> owned = clients[rank].owned;
    map<string,int> liveupdate = clients[rank].liveupdate;
    //mutex 
    // verific daca am segmentul fisierului
    // iar fisierul este cautat in lista de donalodate, lista de 
    // fisiere detinute sau in lista de fisiere in curs de descarcare
    // adica liveupdate 
    pthread_mutex_lock(&dataMutex);
    for (auto &x : owned) {
        if (x.filename == filename) {
            for (auto &seg : x.segments) {
                if (seg == hash) {
                    pthread_mutex_unlock(&dataMutex);
                    return true;
                }
            }
        }
    }
    for (auto &x : downloaded) {
        if (x.filename == filename) {
            for (auto &seg : x.segments) {
                if (seg == hash) {
                    pthread_mutex_unlock(&dataMutex);
                    return true;
                }
            }
        }
    }
    auto it = liveupdate.find(filename);
    if(it != liveupdate.end()){
        if(segindex <= it->second){
        pthread_mutex_unlock(&dataMutex);
        return true;
        }
    }
    pthread_mutex_unlock(&dataMutex);
    return false;

}
void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;
    bool run = true;
    while(run){
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int size;
        MPI_Get_count(&status, MPI_CHAR ,&size);
        int src = status.MPI_SOURCE;
        int tag = status.MPI_TAG;
        if(tag == SEGREQ_TAG){
            // verific daca am segmentul din fisier si daca da trimit o confirmare
            string mes;
            mes.resize(size);
            MPI_Recv(&mes[0],size,MPI_CHAR,src,tag,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
            std::istringstream in(mes);
            string filename,hash;
            int segindex;
            in >> filename >> segindex >>hash;

            bool ok = haveSegm(filename,segindex,hash,rank);
            string msg;
            if(ok == true)
                msg = "OK";
            else
                msg = "NO";
            MPI_Send(msg.data(),msg.size(),MPI_CHAR,src,SEGRESP_TAG,MPI_COMM_WORLD);
        }
        if(tag == TURNOFF_TAG){
            run = false;
        }
    }
    pthread_exit(NULL);
    return NULL;
}

void tracker(int numtasks, int rank) {
    int nr_clients = numtasks - 1;
    int inited = 0;
    int ok = 1;
    int finished = 0;
    while(ok){
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int size;
        MPI_Get_count(&status, MPI_CHAR ,&size);
        int src = status.MPI_SOURCE;
        int tag = status.MPI_TAG;
        string mes;
        mes.resize(size);
        MPI_Recv(&mes[0],size,MPI_CHAR,src,tag,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
        if(tag == INIT_MSG){
            //stochez informatiile de la clienti
            std::istringstream in(mes);
            int nr;
            in >> nr;
            in.ignore();
            for(int i = 0 ; i < nr; i++){
                std::string filename;
                int nrseg;
                in >> filename >> nrseg;
                vector<string> segments(nrseg);
                for(int j = 0; j < nrseg;j++)
                    in >> segments[j];
                if(trackerData.find(filename) != trackerData.end()){
                    trackerData[filename].seeds.push_back(src);
                    trackerData[filename].peers.push_back(src);
                }
                else{
                    TrackerInfo newdata;
                    newdata.filename = filename;
                    newdata.numS = nrseg;
                    newdata.seeds.push_back(src);
                    newdata.peers.push_back(src);
                    newdata.segments = segments;
                    trackerData[filename] = newdata;
                }    
            }
            inited++;
            if(inited == nr_clients){
                // le trimit mesaj ca pot incepe
                string x = "begin";
                for(int i = 1; i < numtasks; i++){
                    MPI_Send(x.data(),x.size(),MPI_CHAR,i,START_TAG,MPI_COMM_WORLD);
                }
            }
        }
        if(tag == REQUEST_TAG){
            //ii trimit swarm ul si de asemnea il adaug clientul in lista de peers
            if (trackerData.find(mes) == trackerData.end()) {
                std::cerr << "Error: File " << mes << " not found in tracker data.\n";
                string msg;
                msg += "nu exista fisierul";
                MPI_Send(msg.data(),msg.size(),MPI_CHAR,src,SWARM_TAG,MPI_COMM_WORLD);
                continue;
            }
            //combin listele de peers si seeds pentru swarm
            vector<int>seeds = trackerData[mes].seeds;
            vector<int>peers = trackerData[mes].peers;
            std::set<int> unique_ranks(seeds.begin(), seeds.end());
            unique_ranks.insert(peers.begin(), peers.end());
            std::vector<int> swarm(unique_ranks.begin(), unique_ranks.end());
            string msg;
            swarm.erase(std::remove(swarm.begin(), swarm.end(), src), swarm.end());
            // compun string ul mesaj
            msg += to_string(swarm.size()) + " ";
            for(int i = 0; i < (int)swarm.size();i++)
                msg += to_string(swarm[i]) + " ";
            vector<string> segments = trackerData[mes].segments;
            msg += to_string(segments.size()) + " ";
            for(int i = 0; i < (int)segments.size();i++)
                msg += segments[i] + " ";
            MPI_Send(msg.data(),msg.size(),MPI_CHAR,src,SWARM_TAG,MPI_COMM_WORLD);
            trackerData[mes].peers.push_back(src);
        }
        if(tag == FINDOWN_TAG){
            string filename = mes;
            // adaug clientul ca seed daca a terminat de downloadat fisierul
            int cnt1 = 0;
            cnt1 = count(trackerData[filename].seeds.begin(),trackerData[filename].seeds.end(),src);
            if(cnt1 == 0)
                trackerData[filename].seeds.push_back(src);
        }
        if(tag == FINAL_TAG){
            // daca toti clientii terminat notific thread-urile upload
            finished++;
            if(finished == nr_clients){
                string msg = "ALLDONE";
                for(int i = 1; i <= nr_clients;i++)
                    MPI_Send(msg.data(), msg.size(), MPI_CHAR, i, TURNOFF_TAG, MPI_COMM_WORLD);
                ok = 0;    
            }
        }
    }
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;
    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}