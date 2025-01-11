# BitTorrent Protocol Simulation

**Author:** Teodor Matei Birleanu  
**Date:** 07.01.2025  
**Contact:** teodor.matei.birleanu@gmail.com

---

## Overview
This project implements a simulation of the BitTorrent protocol using parallel and distributed algorithms. The implementation is designed to demonstrate the core functionalities of BitTorrent, such as file segmentation, peer-to-peer communication, and tracker-based coordination.

---

## Implementation Details

### Data Structures
1. **Tracker Data**:
   - A `map` associating a string (filename) with a structure containing:
     - Filename
     - Number of segments
     - Hashes of segments in order
     - Lists of seeds and peers

2. **File Structure**:
   - Retains:
     - Filename
     - Number of segments
     - A `bool` indicating if the file has been downloaded
     - A vector of segments

3. **Desired Files**:
   - Stores only filenames and a `bool` marking if the file has been downloaded.

4. **Client Structure**:
   - Tracks:
     - Downloaded files
     - Owned files
     - Desired files
   - Includes a `map` (`liveupdate`) to store the last downloaded segment of a file, allowing the client to begin uploading segments immediately after acquisition.

---

## Workflow

### Initialization
1. **Input Reading**:
   - Clients send file information to the tracker to initialize the system.
2. **Message Format**:
   - `"number_of_files" + "filename" + "number_of_segments" + "list_of_segments"`
   - Sent with the `INIT_MSG` tag.
3. **Tracker Initialization**:
   - Uses `MPI_Probe` to receive messages from clients and populates its map with file information.
   - Sends a `START_TAG` message to all clients once initialization is complete.

### File Request and Download
1. **Client Requests**:
   - For each desired file, the client sends the filename to the tracker with the `REQUEST_TAG` tag.
   - Tracker responds with:
     - Swarm size
     - Swarm list
     - Number of segments
     - Segment hashes
     - Response sent with `SWARM_TAG` tag.

2. **Download Process**:
   - Clients request segments from peers in the swarm.
   - If a client cannot provide a segment, it responds with `"NO"`.
   - Messages:
     - Segment requests: `SEGREQ_TAG`
     - Segment responses: `SEGRESP_TAG`
     - Successful download updates `liveupdate` with the latest segment.
   - Completed files are marked as downloaded and reported to the tracker (`FINDOWN_TAG`).

### Upload Process
- Clients respond to segment requests (`SEGREQ_TAG`) by verifying:
  - If the segment is downloaded.
  - If the segment is part of an ongoing download and is available.
- Valid segments are sent to requesting clients.

### Tracker Management
- Tracks active downloads.
- Notifies all upload threads to terminate when downloads are complete (`TURNOFF_TAG`).

---

## Key Features
- Decentralized file sharing with segment-based transfers.
- Dynamic swarm updates to include peers downloading segments.
- Simultaneous upload and download capability.
- Efficient tracker-based file management.

---

## Thank You!
For any questions or feedback, feel free to reach out at **teodor.matei.birleanu@gmail.com**.
