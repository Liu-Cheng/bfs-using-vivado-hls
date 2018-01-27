// s1: read vertex status
for(int i = 0; i < vertexNum; i++){
	inspect_stream << depth[i];
}

// s2: frontier inspection
for(int i = 0; i < vertexNum; i++){
	int d = inspect_stream.read();
	if(d == level){
		frontier_stream << i;
	}
}

// s3: read row pointer array (rpa) of vertices in the frontier
while(frontier_stream.empty() == false){
	int vertexIdx = frontier_stream.read();
	int64_dt data;
	data.range(31, 0) = rpa[vertexIdx];
	data.range(63, 32) = rpa[vertexIdx + 1];
	rpa_stream << data;
}

// s4: read column index array (cia) of vertices in the frontier
while(rpa_stream.empty() == false){
	int data = rpa_stream.read();
	for(int i = data.range(31, 0); i < data.range(63, 32); i++){
		cia_stream << cia[i];
	}
}

// s5: read frontier neighbor status
while(cia_stream.empty() == false){
	int vertexIdx = cia_stream.read();
	int d = depth[vertexIdx];
	neighbor_status_stream << {vertexIdx, d};
}

// s6: analyze next layer frontier
while(neighbor_status_stream.empty() == false){
	int data = neighbor_status_stream.read();
	if(data.range(7, 0) == -1){
		next_frontier_stream << data.range(39, 32);
	}
}

// s7: update depth of next frontier
while(next_frontier_stream.empty() == false){
	int vertexIdx = next_frontier_stream.read();
	depth[vertexIdx] = level + 1;
}
