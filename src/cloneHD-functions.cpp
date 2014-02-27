//cloneHD-functions.cpp

#include "common-functions.h"
#include "cloneHD-functions.h"
#include "clone.h"
#include "emission.h"
#include "minimization.h"

#define PI 3.1415926
#define LOG2 0.693147

using namespace std;


void get_cna_data(Emission * cnaEmit, cmdl_opts& opts, int& nTimes){
  vector<int> chrs;
  vector<int> nSites;
  cnaEmit->mode = (opts.cna_shape > 0.0) ? 4 : 3;
  cnaEmit->shape = opts.cna_shape;
  cnaEmit->log_shape = (opts.cna_shape > 0.0) ? log(opts.cna_shape) : 0.0;
  cnaEmit->rnd_emit  = opts.cna_rnd;
  if (opts.cna_jump >= 0.0)      cnaEmit->connect = 1;
  if (opts.cna_jumps_fn != NULL) cnaEmit->connect = 1;
  get_dims( opts.cna_fn, nTimes, chrs, nSites, cnaEmit->connect);
  cnaEmit->set( nTimes, chrs, nSites, opts.cnaGrid);
  get_data( opts.cna_fn, cnaEmit);
}


void get_baf_data(Emission * bafEmit, cmdl_opts& opts, int& nTimes, int& nT){ 
  vector<int> chrs;
  vector<int> nSites;
  bafEmit->mode = (opts.baf_shape > 0.0) ? 2 : 1;
  bafEmit->shape = opts.baf_shape;
  bafEmit->log_shape = (opts.baf_shape > 0.0) ? log(opts.baf_shape) : 0.0;
  bafEmit->rnd_emit  = opts.baf_rnd;
  bafEmit->reflect = 1;//reflect at 0.5 (n == N-n)
  bafEmit->get_log = 1;
  if (opts.baf_jump >= 0.0)    bafEmit->connect = 1;
  if (opts.baf_jumps_fn!=NULL) bafEmit->connect = 1;
  if (opts.cna_jumps_fn!=NULL) bafEmit->connect = 1;
  get_dims( opts.baf_fn, nT, chrs, nSites, bafEmit->connect);
  if (nTimes > 0 && nT != nTimes){
    cout<<"ERROR in cloneHD main(): incompatible no. samples in CNA and BAF data.\n";
    exit(1);
  }
  nTimes = nT;
  bafEmit->set( nTimes, chrs, nSites, opts.bafGrid);
  get_data( opts.baf_fn, bafEmit);
  bafEmit->set_EmitProb(-1);
}


void get_snv_data(Emission * snvEmit, cmdl_opts& opts, int& nTimes, int& nT){ 
  vector<int> chrs;
  vector<int> nSites;
  snvEmit->mode = (opts.snv_shape > 0.0) ? 2 : 1;
  snvEmit->shape = opts.snv_shape;
  snvEmit->log_shape = (opts.snv_shape > 0.0) ? log(opts.snv_shape) : 0.0;
  snvEmit->rnd_emit = opts.snv_rnd;
  snvEmit->get_log = 1;
  snvEmit->reflect = 0;
  if (opts.snv_jump >= 0.0)    snvEmit->connect = 1;
  if (opts.snv_jumps_fn!=NULL) snvEmit->connect = 1;
  get_dims( opts.snv_fn, nT, chrs, nSites, snvEmit->connect);
  if (nTimes>0 && nT != nTimes){
    cout<<"ERROR in cloneHD main(): incompatible no. samples CNA and SNV data.\n";
    exit(1);
  }
  nTimes = nT;
  snvEmit->set( nTimes, chrs, nSites, opts.snvGrid);
  get_data( opts.snv_fn, snvEmit);
  snvEmit->set_EmitProb(-1);
}

void get_snv_bulk_prior(Clone * myClone, cmdl_opts& opts){
  if (opts.bulk_mean)  myClone->allocate_bulk_mean();
  if (opts.bulk_prior) myClone->allocate_bulk_dist();
  double ** vardummy  = NULL;
  gsl_matrix ** distdummy = NULL;
  if (opts.bulk_mean){
    get_track( opts.bulk_fn, distdummy, myClone->bulk_prior_mean, vardummy, myClone->snvEmit);
  }
  else if (opts.bulk_prior){
    get_track(opts.bulk_fn,myClone->bulk_prior,myClone->bulk_prior_mean,vardummy,myClone->snvEmit);
  }
  else{
    abort();
  }
}

//general function to read in mean, variance and distribution...
void get_track(const char * track_fn, 
	       gsl_matrix **& distribution,  
	       double **& mean,
	       double **& var,
	       Emission * myEmit
	       ){
  ifstream ifs;
  string line;
  stringstream line_ss;
  ifs.open( track_fn, ios::in);
  if (ifs.fail()){
    printf("ERROR in get_track(): file %s cannot be opened.\n", track_fn);
    exit(1);
  }
  if (mean == NULL){
    printf("ERROR-1 in get_track().\n");
    exit(1);
  }
  int chr = 0, old_chr = -1, l=0, locus;
  double mn, sd, jp;
  while( ifs.good() ){
    line.clear();
    getline( ifs, line);
    if (line.empty()) break;
    if (line[0] == '#') continue;
    line_ss.clear();
    line_ss.str(line);
    line_ss >> chr >> locus; 
    l = (chr == old_chr) ? l+1 : 0;
    if (chr != old_chr && (chr > myEmit->maxchr || myEmit->idx_of[chr] < 0) ){
      printf("ERROR 1a in get_track(): unexpected chromosome.\n");
      cout<<line<<endl;
      exit(1);
    }
    // exit(0);
    old_chr = chr;
    if( (int) myEmit->loci[ myEmit->idx_of[chr] ][l] != locus){
      printf("ERROR 2 in get_track()\n");
      cout<<line<<endl;
      printf( "%i, %i, %i vs %i\n", myEmit->idx_of[chr], l, myEmit->loci[ myEmit->idx_of[chr] ][l], locus);
      exit(1);
    }
    line_ss >> mn;
    mean[ myEmit->idx_of[chr] ][l] = mn;
    if (var != NULL) {
      line_ss >> sd;
      var[ myEmit->idx_of[chr] ][l]  = pow(sd,2);
    }
    if (distribution == NULL) continue;
    double p;
    line_ss >> jp;
    for (int i=0; i < (int) (distribution[ myEmit->idx_of[chr] ])->size2; i++){
      if (line_ss.good() != true){
        printf("ERROR 3 in get_track()\n");
        exit(1);
      }
      line_ss >> p;
      gsl_matrix_set( distribution[ myEmit->idx_of[chr] ], l, i, p);
    }
  }
  ifs.close();
}


// get the maximum total c.n. per chromosome
void get_maxtcn_input(const char * maxtcn_fn, int maxtcn_gw, Clone * myClone){
  myClone->maxtcn_input.clear();
  myClone->all_maxtcn.clear();
  myClone->maxtcn = 0;
  if (maxtcn_fn != NULL){
    ifstream ifs;
    string line;
    stringstream line_ss;
    ifs.open( maxtcn_fn, ios::in);
    if (ifs.fail()){
      printf("ERROR: file %s cannot be opened.\n", maxtcn_fn);
      exit(1);
    }
    int chr = 0, maxtcn=0;   
    while( ifs.good() ){
      line.clear();
      getline( ifs, line);
      if (line.empty()) break;
      if (line[0] == '#') continue;
      line_ss.clear();
      line_ss.str(line);
      line_ss >> chr;
      if ( myClone->maxtcn_input.count(chr) == 0){
        vector<int> mx;
        while( line_ss.good() ){
         line_ss >> maxtcn;
         mx.push_back(maxtcn);
         myClone->all_maxtcn.insert(maxtcn);
         myClone->maxtcn = max( myClone->maxtcn, maxtcn);
        }  
        myClone->maxtcn_input.insert(std::pair<int, vector<int> >(chr,mx) );
      }
      else{
       printf("ERROR: in file %s: chr %i appears twice.\n", maxtcn_fn, chr);
       exit(1);
     }
    }
    ifs.close();
    //check that all chromosomes are covered
    if ( myClone->cnaEmit->is_set){
      for (int s=0; s< myClone->cnaEmit->nSamples; s++){
       int cnaChr = myClone->cnaEmit->chr[s];
       if ( myClone->maxtcn_input.count(cnaChr) == 0){
         printf("ERROR: in file %s: chr %i is missing.\n", maxtcn_fn, cnaChr);
         exit(1);
       }
      }
    }
    else if ( myClone->snvEmit->is_set){
      for (int s=0; s< myClone->snvEmit->nSamples; s++){
        int snvChr = myClone->snvEmit->chr[s];
        if ( myClone->maxtcn_input.count(snvChr) == 0){
         printf("ERROR: in file %s: chr %i is missing.\n", maxtcn_fn, snvChr);
         exit(1);
       }
      }
    }
  }
  else if (myClone->cnaEmit->is_set){
    for (int s=0; s< myClone->cnaEmit->nSamples; s++){
      int cnaChr = myClone->cnaEmit->chr[s];
      int maxtcn = (maxtcn_gw > 0) ? maxtcn_gw : myClone->normal_copy[cnaChr];
      vector<int> mx;
      mx.push_back(maxtcn);
      myClone->maxtcn_input.insert(std::pair<int, vector<int> >(cnaChr,mx));
      myClone->all_maxtcn.insert(maxtcn);
      myClone->maxtcn = max( myClone->maxtcn, maxtcn);
    }
  }    
  else if ( myClone->snvEmit->is_set){
    for (int s=0; s< myClone->snvEmit->nSamples; s++){
      int snvChr = myClone->snvEmit->chr[s];
      int maxtcn = (maxtcn_gw > 0) ? maxtcn_gw : myClone->normal_copy[snvChr];
      vector<int> mx;
      mx.push_back(maxtcn);
      myClone->maxtcn_input.insert(std::pair<int, vector<int> >(snvChr,mx));
      myClone->all_maxtcn.insert(maxtcn);
      myClone->maxtcn = max( myClone->maxtcn, maxtcn);
    }
  }
  if ( myClone->maxtcn == 0) abort();
  printf("A maximum total copy number of %i will be used.\n\n", myClone->maxtcn);
}



//get total copynumber tracks from file...
void get_mean_tcn( const char * mntcn_fn, Clone * myClone, Emission * myEmit){
  ifstream ifs;
  string line;
  stringstream line_ss;
  ifs.open( mntcn_fn, ios::in);
  if (ifs.fail()){
    printf("ERROR file %s cannot be opened.\n", mntcn_fn);
    exit(1);
  }
  int chr=0, old_chr = -1;
  int cn_locusi=0, cn_locusf=0, nloci=0, locus=-1;
  int evt=0;
  double mcn, x;
  double * buff = new double [myEmit->nTimes];
  int sample=-1, perevt=0;
  int wait=0;
  while( ifs.good() ){
    line.clear();
    getline( ifs, line);
    if (line.empty()) break;
    if (line[0] == '#') continue;
    line_ss.clear();
    line_ss.str(line);
    if (old_chr == -1 && locus == -1){//check format
      int cols=0;
      while(line_ss >> x) cols++;
      if (cols == 1 + 3 + myEmit->nTimes){
        perevt = 1;
      }
      else if (cols != 1 + 1 + myEmit->nTimes){
        printf("ERROR: check format in %s.\n", mntcn_fn);
        exit(1);
      }
      line_ss.clear();//reset
      line_ss.str(line);
    }
    line_ss >> chr >> cn_locusi;
    if (perevt){
      line_ss >> nloci >> cn_locusf;
    }
    else{
      cn_locusf = cn_locusi;
      nloci = 1;
    }
    if (chr != old_chr){//new chromosome
      if(myEmit->chrs.count(chr) == 0){
        wait=1;
      }
      else{
        wait=0;
      }
      if (old_chr != -1 ){//not the first new chr
	sample = myEmit->idx_of[old_chr];
	if ( evt < myEmit->nEvents[sample]){//remaining entries from last chr
	  int oevt = evt-1;
	  for (int e=evt; e<myEmit->nEvents[sample]; e++){
	    for (int t=0; t<myEmit->nTimes; t++){
	      myEmit->mean_tcn[t][sample][e] = myEmit->mean_tcn[t][sample][oevt];
	    }
	  }
	}
	evt = 0;
      }
    }
    old_chr = chr;
    if (wait) continue;
    sample  = myEmit->idx_of[chr];
    if ( evt >= myEmit->nEvents[sample]) continue;//chromosome is complete!
    locus = (int) myEmit->loci[sample][myEmit->idx_of_event[sample][evt]];//current target locus
    if( cn_locusf <  locus) continue;
    // now we are above or at next event...
    for (int t=0; t<myEmit->nTimes; t++){
      if (line_ss.good() != true) abort();
      line_ss >> mcn;
      buff[t] = mcn;
    }
    //fill
    while(locus <= cn_locusf){
      for (int t=0; t<myEmit->nTimes; t++) myEmit->mean_tcn[t][sample][evt] = buff[t];
      evt++;
      if ( evt >= myEmit->nEvents[sample]) break;
      locus = (int) myEmit->loci[sample][myEmit->idx_of_event[sample][evt]];
    }
  }
  //last bit
  sample = myEmit->idx_of[old_chr];
  if (evt < myEmit->nEvents[sample]){
    int oevt = evt-1;
    for (int e=evt; e<myEmit->nEvents[sample]; e++){
      for (int t=0; t<myEmit->nTimes; t++){
	myEmit->mean_tcn[t][sample][e] = myEmit->mean_tcn[t][sample][oevt];
      }
    }
  }
  //done
  ifs.close();
}




//get copy number availability
void get_avail_cn( const char * avcn_fn, Clone * myClone, Emission * myEmit){
  ifstream ifs;
  string line;
  stringstream line_ss;
  ifs.open( avcn_fn, ios::in);
  if (ifs.fail()){
    printf("ERROR file %s cannot be opened.\n", avcn_fn);
    exit(1);
  }
  int chr=0, old_chr = -1;
  int cn_locusi=0, cn_locusf=0, nloci=0, locus=-1;
  int evt=0;
  double frac, x;
  int nEl = (myClone->maxtcn+1)*myClone->nTimes;
  double ** buff = new double * [myClone->nTimes];
  for (int t=0; t<myClone->nTimes; t++) buff[t] = new double [myClone->maxtcn+1];
  int sample=-1, perevt=0;
  int wait=0;
  while( ifs.good() ){
    line.clear();
    getline( ifs, line);
    if (line.empty()) break;
    if (line[0] == '#') continue;
    line_ss.clear();
    line_ss.str(line);
    if (old_chr == -1 && locus == -1){//check format
      int cols=0;
      while(line_ss >> x) cols++;
      if (cols == 1 + 3 + nEl){
	perevt = 1;
      }
      else if (cols != 1 + 1 + nEl){
	printf("ERROR: check format in %s.\n", avcn_fn);
	exit(1);
      }
      line_ss.clear();//reset
      line_ss.str(line);
    }
    line_ss >> chr >> cn_locusi;
    if (perevt){
      line_ss >> nloci >> cn_locusf;
    }
    else{
      cn_locusf = cn_locusi;
      nloci = 1;
    }
    if (chr != old_chr){//new chromosome
      if(myEmit->chrs.count(chr) == 0){
	wait = 1;
      }
      else{
	wait = 0;
      }
      if (old_chr != -1 ){//not the first new chr
	sample = myEmit->idx_of[old_chr];
	if ( evt < myEmit->nEvents[sample]){//remaining from last chr
	  int oevt = evt-1;
	  for (int e=evt; e<myEmit->nEvents[sample]; e++){
	    for (int t=0; t<myEmit->nTimes; t++){
	      for (int cn=0; cn<=myClone->maxtcn; cn++){
		myEmit->av_cn[t][sample][e][cn] = myEmit->av_cn[t][sample][oevt][cn];
	      }
	    }
	  }
	}
	evt = 0;
      }
    }
    old_chr = chr;
    if (wait) continue;
    sample  = myEmit->idx_of[chr];
    if ( evt >= myEmit->nEvents[sample]) continue;//chromosome is complete!
    locus = (int) myEmit->loci[sample][myEmit->idx_of_event[sample][evt]];//current target locus
    if( cn_locusf <  locus) continue;
    // now we are above or at next event...
    for (int t=0; t<myEmit->nTimes; t++){
      for (int cn=0; cn<=myClone->maxtcn; cn++){
	if (line_ss.good() != true) abort();
	line_ss >> frac;
	buff[t][cn] = frac;
      }
    }
    // now fill
    while(locus <= cn_locusf){
      for (int t=0; t<myEmit->nTimes; t++){
	for (int cn=0; cn<=myClone->maxtcn; cn++){
	  myEmit->av_cn[t][sample][evt][cn] = buff[t][cn];
	}
      }
      evt++;
      if ( evt >= myEmit->nEvents[sample]) break;
      locus = (int) myEmit->loci[sample][myEmit->idx_of_event[sample][evt]];
    }
  }
  //last bit
  sample = myEmit->idx_of[old_chr];
  if (evt < myEmit->nEvents[sample]){
    int oevt = evt-1;
    for (int e=evt; e<myEmit->nEvents[sample]; e++){
      for (int t=0; t<myEmit->nTimes; t++){
	for (int cn=0; cn<=myClone->maxtcn; cn++){
	  myEmit->av_cn[t][sample][e][cn] = myEmit->av_cn[t][sample][oevt][cn];
	}
      }
    }
  }
  //done
  ifs.close();
}





//read in frequencies to act as lower limit in minimisations...
void get_purity( const char * purity_fn, gsl_vector *& purity){
  printf("Using data in %s for sample purities (lower bounds for subclone frequencies)...\n", purity_fn);
  ifstream ifs;
  string line;
  stringstream line_ss;
  ifs.open( purity_fn, ios::in);
  if (ifs.fail()){
    printf("ERROR: file %s cannot be opened.\n", purity_fn);
    exit(1);
  }
  double x;
  int ct=0;
  while( ifs.good() ){
    line.clear();
    getline( ifs, line);
    if (line.empty()) break;
    line_ss.clear();
    line_ss.str(line);
    line_ss >> x;
    if (ct >= (int) purity->size){
      cout<<"ERROR-2 in get_purity()\n"<<endl;
      exit(1);
    }
    else{
      purity->data[ct] = x;
    }
    ct++;
  }
  ifs.close();
  if (ct != (int) purity->size){
    cout<<"ERROR-3 in get_purity()\n"<<endl;
    exit(1);
  }
}


//read in clone frequencies and mass parameters from file...
void get_fixed_clones(gsl_matrix *& clones, gsl_vector *& mass, const char * clones_fn, int nT){
  printf("Using data in %s as input...\n", clones_fn);
  ifstream ifs;
  string line;
  stringstream line_ss;
  ifs.open( clones_fn, ios::in);
  if (ifs.fail()){
    printf("ERROR: file %s cannot be opened.\n", clones_fn);
    exit(1);
  }
  //
  string first;
  double x;
  int rows=0,cols=0,ocols=0;
  int with_mass = 0;
  while( ifs.good() ){//get dimensions...
    line.clear();
    getline( ifs, line);
    if (line.empty()) break;
    if (line[0] == '#') continue;
    line_ss.clear();
    line_ss.str(line);
    cols = 0;
    while(line_ss >> x){
      if (cols==0 && x>1.0) with_mass = 1;
      cols++;
    }
    if (ocols > 0 && cols != ocols){
      cout<<"ERROR in get_fixed_clones(): column sizes not consistent\n";
      exit(1);
    }
    else{
      ocols = cols;
    }
    rows++;
  }
  ifs.close();
  //allocate...
  if (rows < nT){
    printf("ERROR: not enough lines in file %s for the given data.\n", clones_fn);
    exit(1);
  }
  else if ( 0 != rows % nT){
    printf("ERROR: no. lines in file %s incompatible with the given data.\n", clones_fn);
    exit(1);
  }
  if ( cols - with_mass > 0 ) clones = gsl_matrix_alloc( rows, cols - with_mass);
  if (with_mass==1) mass = gsl_vector_alloc(rows);
  if (mass==NULL && clones==NULL){
    printf("ERROR: file %s seems corrupted (expect matrix).\n", clones_fn);
    exit(1);
  }
  ifs.open( clones_fn, ios::in);
  rows=0;
  while( ifs.good() ){
    line.clear();
    getline( ifs, line);
    if (line.empty()) break;
    if (line[0] == '#') continue;
    line_ss.clear();
    line_ss.str(line);
    cols = 0;
    while(line_ss >> x){
      if (with_mass == 1){
	if (cols == 0){
	  gsl_vector_set( mass, rows, x);
	}
	else{
	  gsl_matrix_set( clones, rows, cols-1, x);
	}
      }
      else{
	gsl_matrix_set( clones, rows, cols, x);
      }
      cols++;
    }
    rows++;
  }
  ifs.close();
  rows = (mass != NULL) ? (int) mass->size : (int) clones->size1;
  if (rows == nT){//print for single input...
    for (int i=0; i<rows; i++){
      if (mass != NULL) printf("%e ", mass->data[i]);
      if (clones!=NULL){
	for (int j=0; j<(int)clones->size2; j++){
	  printf("%e ", gsl_matrix_get(clones,i,j));
	}
      }
      cout<<endl;
    }
    cout<<endl;
  }
}



//***posterior jump probability track***
void get_jump_probability( Clone * myClone, cmdl_opts& opts){
  Emission * cnaEmit = myClone->cnaEmit;
  Emission * bafEmit = myClone->bafEmit;
  Emission * snvEmit = myClone->snvEmit; 
  double ** vardummy = NULL;
  gsl_matrix ** distdummy = NULL;
  if (cnaEmit->is_set){//***CNA JUMPS***
    if(opts.cna_jumps_fn != NULL){//1. either use external jump probability track
      get_track( opts.cna_jumps_fn, distdummy, cnaEmit->pjump, vardummy, cnaEmit);
      for (int s=0; s< cnaEmit->nSamples; s++){
	cnaEmit->coarse_grain_jumps( s, opts.min_jump, 5);
      }
      cnaEmit->get_events_via_jumps();
    }
    else if (opts.cna_jump >= 0.0){//2. or constant jump probability per base
      cnaEmit->set_pjump(opts.cna_jump);
    }
    else{
      abort();
    }
    //map CNA events to BAF and SNV...
    if(bafEmit->is_set){
      for (int s=0; s<bafEmit->nSamples; s++){
	bafEmit->map_idx_to_Event( cnaEmit, s);
      }
    }
    if (snvEmit->is_set){
      for (int s=0; s<snvEmit->nSamples; s++){
	if ( !bafEmit->is_set ||bafEmit->chrs.count(snvEmit->chr[s]) == 0){
	  snvEmit->map_idx_to_Event( cnaEmit, s);
	}
      }
    }
  }
  if ( bafEmit->is_set ){//***BAF JUMPS***
    if (opts.baf_jumps_fn != NULL){//1. either external jump probability track
      get_track( opts.baf_jumps_fn, distdummy, bafEmit->pjump, vardummy, bafEmit);
      for (int s=0; s< bafEmit->nSamples; s++){// ignore improbable jump events
	bafEmit->coarse_grain_jumps( s, opts.min_jump, 5);
      }
      bafEmit->get_events_via_jumps();
      if ( cnaEmit->is_set && opts.cna_jumps_fn != NULL ){//allow transit at CNA jumps
	bafEmit->add_break_points_via_jumps( cnaEmit, opts.min_jump);
      }
      bafEmit->get_events_via_jumps();
    }
    else if ( cnaEmit->is_set && opts.cna_jumps_fn != NULL ){//2. map the CNA jumps to BAF
      bafEmit->map_jumps(cnaEmit);
      bafEmit->get_events_via_jumps();
    }
    else if (opts.baf_jump >= 0.0){//or 3. constant jump probability per base
      bafEmit->set_pjump(opts.baf_jump);
    }
    else{
      abort();
    }
    // map BAF events to SNV
    if (snvEmit->is_set){
      for (int s=0; s<snvEmit->nSamples; s++){
	if (bafEmit->chrs.count(snvEmit->chr[s]) == 1){
	  snvEmit->map_idx_to_Event( bafEmit, s);
	}
      }
    }
  }
  if ( snvEmit->is_set ){//***SNV JUMPS***
    if ( opts.snv_jumps_fn != NULL ){
      get_track( opts.snv_jumps_fn, distdummy, snvEmit->pjump, vardummy, snvEmit);
      for (int s=0; s< snvEmit->nSamples; s++){//ignore improbable jump events
	snvEmit->coarse_grain_jumps( s, opts.min_jump, 5);
      }
      snvEmit->get_events_via_jumps();
      if ( opts.cna_jumps_fn != NULL ){//allow SNV to jump at CNA jump sites
	snvEmit->add_break_points_via_jumps( cnaEmit, opts.min_jump);
      }
      snvEmit->get_events_via_jumps();
    }
    else if (opts.snv_jump >= 0.0){
      snvEmit->set_pjump(opts.snv_jump);
    }    
  }
  // allocations, now that all events are fixed...
  if (cnaEmit->is_set){
    cnaEmit->allocate_mean_tcn();
    if (bafEmit->is_set){
      bafEmit->allocate_mean_tcn();
      bafEmit->allocate_av_cn(myClone->maxtcn);
    }
    else{
      cnaEmit->allocate_av_cn(myClone->maxtcn);
    }
    if (snvEmit->is_set) snvEmit->allocate_mean_tcn();
  }
  else if (snvEmit->is_set){
    if (opts.mntcn_fn != NULL) snvEmit->allocate_mean_tcn();
    if (opts.avcn_fn  != NULL) snvEmit->allocate_av_cn(myClone->maxtcn);
  }
}




//***BIAS FIELD***
void get_bias_field( Clone * myClone, cmdl_opts& opts){
  Emission * cnaEmit = myClone->cnaEmit;
  cnaEmit->allocate_bias();
  get_bias( opts.bias_fn, cnaEmit);
  //normalize the bias field...
  double bias_mean=0.0,norm=0.0;
  for (int s=0; s< cnaEmit->nSamples; s++){
    int cnaChr = cnaEmit->chr[s];
    double ncn = (double) myClone->normal_copy[cnaChr];
    if (ncn==0) abort();
    for (int i=0; i< cnaEmit->nSites[s]; i++){
      bias_mean += cnaEmit->bias[s][i] / ncn;
    }
    norm += double(cnaEmit->nSites[s]);
  }
  if (norm==0) abort();
  if (bias_mean==0) abort();
  bias_mean /= norm;
  for (int s=0; s< cnaEmit->nSamples; s++){
    int cnaChr = cnaEmit->chr[s];
    double ncn = (double) myClone->normal_copy[cnaChr];
    for (int i=0; i< cnaEmit->nSites[s]; i++){
      cnaEmit->bias[s][i] /= bias_mean * ncn;
      cnaEmit->log_bias[s][i] = log(cnaEmit->bias[s][i]);
      for (int t=1; t<myClone->cnaEmit->nTimes; t++){
	cnaEmit->bias[s][i]     = cnaEmit->bias[s][i];
	cnaEmit->log_bias[s][i] = cnaEmit->log_bias[s][i];
      }
    }
  }
}



void print_llh_for_set(gsl_matrix * clones, gsl_vector * mass, Clone * myClone, cmdl_opts& opts){
  if (clones == NULL){
    cout<<"ERROR: all parameters have to be specified.\n";
    exit(1);
  }
  char clonal_out[1024];
  sprintf( clonal_out, "%s.llh-values.txt", opts.pre);
  FILE * clonal_fp = fopen(clonal_out,"w");
  fprintf(clonal_fp, "# cna-llh baf-llh snv-llh total-llh\n");
  int rows = (int) clones->size1;
  myClone->nClones = (int) clones->size2;
  int nC = myClone->nClones;
  int nT = myClone->nTimes;
  int nPts = rows / nT; 
  gsl_matrix_view nclones;
  gsl_vector_view nmass;
  printf("Printing log-likelihood values for %i parameter sets to %s\n", nPts, clonal_out);
  if (myClone->cnaEmit->is_set && mass != NULL){        
    for (int i=0; i<nPts; i++){
      printf("\r%i", i+1);
      cout<<flush;
      nclones = gsl_matrix_submatrix( clones, i*nT, 0, nT, nC);
      nmass   = gsl_vector_subvector( mass, i*nT, nT); 
      myClone->set( &nclones.matrix );		
      myClone->set_mass( &nmass.vector );
      myClone->get_all_total_llh();
      fprintf( clonal_fp, "%.8e %.8e %.8e %.8e\n", 
	       myClone->cna_total_llh , myClone->baf_total_llh , myClone->snv_total_llh,
	       myClone->cna_total_llh + myClone->baf_total_llh + myClone->snv_total_llh);
    }
  }
  else if ( myClone->snvEmit->is_set){   
    fprintf(clonal_fp, "\n");  
    for (int i=0; i<nPts; i++){
      printf("\r%i", i+1);
      cout<<flush;
      nclones = gsl_matrix_submatrix( clones, i*nT, 0, nT, nC);
      myClone->set(&nclones.matrix);		
      myClone->get_snv_total_llh();
      fprintf( clonal_fp, "%.8e %.8e %.8e %.8e\n", 
	       0. , 0. , myClone->snv_total_llh,
	       myClone->snv_total_llh);
    }
  }
  cout<<"...done"<<endl;
  fclose(clonal_fp);
}


// *** PRINT ALL RESULTS ***
void  print_all_results( Clone * myClone, cmdl_opts& opts){
  Emission * cnaEmit = myClone->cnaEmit;
  Emission * bafEmit = myClone->bafEmit;
  Emission * snvEmit = myClone->snvEmit;
  bafEmit->connect = 0;
  myClone->symmetrize_baf=1;
  // margin-map to get the posterior per clone...
  char buff[1024]; 
  sprintf( buff, "%s.clonal.margin_map.txt", opts.pre);
  FILE * margin_fp = fopen( buff, "w");
  for (int i=0; i<(int) myClone->margin_map->size1; i++){
    for (int j=0; j<(int) myClone->margin_map->size2; j++){
      fprintf(margin_fp, "%.1f ", gsl_matrix_get(myClone->margin_map,i,j));
    }
    fprintf(margin_fp, "\n");
  }
  fclose(margin_fp);
  // used total copynumber...
  FILE * baf_utcn_fp=NULL, * snv_utcn_fp=NULL; 
  if ( bafEmit->is_set ){
    sprintf( buff, "%s.baf.used_mean_tcn.txt", opts.pre);
    baf_utcn_fp = fopen( buff, "w");
    fprintf( baf_utcn_fp, "#chr site used-mean-total-copynumber\n");
  }
  if ( snvEmit->is_set ){
    sprintf( buff, "%s.snv.used_mean_tcn.txt", opts.pre);
    snv_utcn_fp = fopen( buff, "w");
    fprintf( snv_utcn_fp, "#chr site used-mean-total-copynumber\n");
  }
  // print CNA posterior distributions..
  if (cnaEmit->is_set){
    sprintf( buff, "%s.cna.posterior.txt", opts.pre);
    FILE * cna_fp = fopen( buff, "w");
    print_clonal_header( cna_fp, myClone, cnaEmit, opts);
    sprintf( buff,"%s.mean_tcn.txt", opts.pre); 
    FILE * mntcn_fp =  fopen(buff,"w");
    fprintf( mntcn_fp, "#chr site mean-total-copynumber\n");
    sprintf( buff,"%s.available_cn.txt", opts.pre); 
    FILE * avcn_fp =  fopen(buff,"w");
    fprintf( avcn_fp, "#chr site copynumber-availability\n");
    //allocate space for the posterior
    myClone->alpha_cna = new gsl_matrix * [cnaEmit->nSamples];
    myClone->gamma_cna = new gsl_matrix * [cnaEmit->nSamples];
    for (int s=0; s < cnaEmit->nSamples; s++){//print each chromosome...
      myClone->alpha_cna[s] = NULL;
      myClone->gamma_cna[s] = NULL;
      myClone->get_cna_posterior(s);
      print_posterior( cna_fp, myClone, cnaEmit, s, opts);
      //get and print mean total copynumber...
      myClone->get_mean_tcn(s);
      print_mean_tcn( mntcn_fp, myClone, cnaEmit, s, opts);
      if (!bafEmit->is_set){//get cn availability via CNA...
	myClone->get_avail_cn( cnaEmit, s);      
	print_avail_cn( avcn_fp, myClone, cnaEmit, s, opts);
      }
      int cnaChr = cnaEmit->chr[s];
      int bafSample = -1;
      int snvSample = -1;
      //map mean total cn...
      if (bafEmit->is_set && bafEmit->chrs.count(cnaChr) == 1){
	myClone->map_mean_tcn( cnaEmit, s, bafEmit);
	bafSample = bafEmit->idx_of[cnaChr];
	if (snvEmit->is_set && snvEmit->chrs.count(cnaChr) == 1){
	  myClone->map_mean_tcn( bafEmit, bafSample, snvEmit);
	  snvSample = snvEmit->idx_of[cnaChr];
	}
      }
      else if (snvEmit->is_set && snvEmit->chrs.count(cnaChr) == 1){
	myClone->map_mean_tcn( cnaEmit, s, snvEmit);
	snvSample = snvEmit->idx_of[cnaChr];
      }     
      //print used total-copynumber tracks...
      if ( bafSample >= 0 && bafEmit->mean_tcn != NULL){
	print_mean_tcn( baf_utcn_fp,  myClone, bafEmit, bafSample, opts);
      }
      if ( snvSample >= 0 && snvEmit->mean_tcn != NULL){
	print_mean_tcn( snv_utcn_fp,  myClone, snvEmit, snvSample, opts);
      }
    }//...all CNA posteriors printed.
    if (bafEmit->is_set){//print BAF posterior...
      sprintf( buff, "%s.baf.posterior.txt", opts.pre);
      FILE * baf_fp = fopen( buff, "w");
      print_clonal_header( baf_fp, myClone, bafEmit, opts);
      myClone->alpha_baf = new gsl_matrix * [bafEmit->nSamples];
      myClone->gamma_baf = new gsl_matrix * [bafEmit->nSamples];  
      for (int s=0; s < bafEmit->nSamples; s++){
	myClone->alpha_baf[s] = NULL;
	myClone->gamma_baf[s] = NULL;
	myClone->get_baf_posterior(s);
	print_posterior( baf_fp, myClone, bafEmit, s, opts);
	//get cn availability via BAF...
	myClone->get_avail_cn( bafEmit, s);      
	print_avail_cn( avcn_fp, myClone, bafEmit, s, opts);
      }
      fclose(baf_fp);
    }
    fclose(cna_fp);
    fclose(mntcn_fp);
    fclose(avcn_fp);
    if (snvEmit->is_set){//print SNV posterior...
      sprintf( buff, "%s.snv.posterior.txt", opts.pre);
      FILE * snv_fp = fopen( buff, "w");
      print_clonal_header( snv_fp, myClone, snvEmit, opts);
      myClone->alpha_snv = new gsl_matrix * [snvEmit->nSamples];
      myClone->gamma_snv = new gsl_matrix * [snvEmit->nSamples];   
      for (int s=0; s < snvEmit->nSamples; s++){
	myClone->alpha_snv[s] = NULL;
	myClone->gamma_snv[s] = NULL;
	myClone->get_snv_posterior(s);
	print_posterior( snv_fp, myClone, snvEmit, s, opts);
	gsl_matrix_free(myClone->gamma_snv[s]);
	myClone->gamma_snv[s] = NULL;
      }
      fclose(snv_fp);
      delete [] myClone->gamma_snv;
      delete [] myClone->alpha_snv;
    }
    // clean up posterior...
    for (int s=0; s < cnaEmit->nSamples; s++) gsl_matrix_free(myClone->gamma_cna[s]);
    delete []  myClone->gamma_cna;
    myClone->gamma_cna = NULL;
    if (bafEmit->is_set){
      for (int s=0; s < bafEmit->nSamples; s++) gsl_matrix_free(myClone->gamma_baf[s]);
      delete [] myClone->gamma_baf;
      delete [] myClone->alpha_baf;
    }
  } 
  else if (snvEmit->is_set){//SNV only...
    sprintf( buff, "%s.snv.posterior.txt", opts.pre);
    FILE * snv_fp = fopen( buff, "w");
    print_clonal_header( snv_fp, myClone, snvEmit, opts);
    myClone->alpha_snv = new gsl_matrix * [snvEmit->nSamples];
    myClone->gamma_snv = new gsl_matrix * [snvEmit->nSamples];    
    for (int s=0; s < snvEmit->nSamples; s++){
      myClone->alpha_snv[s] = NULL;
      myClone->gamma_snv[s] = NULL;
      myClone->get_snv_posterior(s);
      print_posterior( snv_fp, myClone, snvEmit, s, opts);
      gsl_matrix_free( myClone->gamma_snv[s]);
      myClone->gamma_snv[s] = NULL;
      //used mean total copynumber...
      print_mean_tcn( snv_utcn_fp,  myClone, snvEmit, s, opts);
    }
    fclose(snv_fp);
    delete [] myClone->gamma_snv;
    delete [] myClone->alpha_snv;
  }
  if (snv_utcn_fp != NULL) fclose(snv_utcn_fp);
  if (baf_utcn_fp != NULL) fclose(baf_utcn_fp);
  //SNV BULK
  if (snvEmit->is_set && myClone->bulk_mean != NULL){
    if (opts.bulk_updates > 0) myClone->set_bulk_to_post();
    //print bulk posterior mean/distribution//TBD
    for (int t=0; t<myClone->nTimes; t++){
      sprintf( buff, "%s.bulk.posterior.%i.txt", opts.pre, t+1);
      FILE * bulk_fp = fopen( buff, "w");
      fprintf( bulk_fp, "#sample site mean");
      if (myClone->bulk_prior!=NULL){
	fprintf( bulk_fp, " std-dev dist");
      }
      fprintf( bulk_fp, "\n");
      for (int s=0; s < snvEmit->nSamples; s++){
	for (int idx=0; idx<snvEmit->nSites[s]; idx++){
	  double bmean = myClone->bulk_mean[t][s][idx];
	  fprintf( bulk_fp, "%i %i %.3f", snvEmit->chr[s], snvEmit->loci[s][idx], bmean);
	  if (myClone->bulk_prior != NULL){
	    gsl_vector_view bdist = gsl_matrix_row( myClone->bulk_dist[t][s], idx);
	    double var = get_var( &bdist.vector, 0.0, 1.0, bmean);
	    fprintf( bulk_fp, " %.3f", sqrt(var));
	    for (int i=0; i<= myClone->bulkGrid; i++){
	      fprintf( bulk_fp, " %.3f", (&bdist.vector)->data[i]);
	    }
	  }
	  fprintf( bulk_fp, "\n");
	}	
      }
      fclose(bulk_fp);
    }
  }
}


void print_clonal_header(FILE * fp, Clone * myClone, Emission * myEmit, cmdl_opts& opts){
  fprintf( fp,"#copynumbers:\n");
  for (int k=0; k < myClone->nLevels; k++){
    for (int f=0; f<myClone->nClones; f++){
      fprintf( fp, "%i", myClone->copynumber[k][f]);
    }
    fprintf( fp," ");
  }
  fprintf( fp,"\n#clonal frequencies:\n");
  for (int t=0; t<myClone->nTimes; t++){
    for (int k=0; k<myClone->nLevels; k++){
      fprintf( fp, "%.3e ", myClone->clone_spectrum[t][k]);
    }
    fprintf( fp, "\n");
  }
  if (opts.print_all || myEmit->coarse_grained == 0 ){//HERE!!!
    fprintf( fp,"#chr locus PostDist\n");
  }
  else{
    fprintf( fp,"#chr first-locus nloci last-locus PostDist\n");
  }
}


void print_posterior( FILE * post_fp, Clone * myClone, Emission * myEmit, int s, cmdl_opts& opts){
  for (int evt=0; evt < myEmit->nEvents[s]; evt++){   
    int first = myEmit->idx_of_event[s][evt];     
    int last  = (evt < myEmit->nEvents[s]-1) ?  myEmit->idx_of_event[s][evt+1] - 1 : myEmit->nSites[s]-1; 
    gsl_matrix * post=NULL;
    if ( myEmit == myClone->cnaEmit) post =  myClone->gamma_cna[s];
    if ( myEmit == myClone->bafEmit) post =  myClone->gamma_baf[s];
    if ( myEmit == myClone->snvEmit) post =  myClone->gamma_snv[s];
    if( opts.print_all || myEmit->coarse_grained == 0 ){
      for (int idx=first; idx<=last; idx++){
      	fprintf( post_fp, "%i %6i", myEmit->chr[s], myEmit->loci[s][idx]);
        for (int j=0; j< myClone->nLevels; j++){
          fprintf( post_fp, " %.3f", gsl_matrix_get( post, evt, j));
      	}
      	fprintf( post_fp,"\n");
      }
    }
    else{
      fprintf( post_fp, "%i %6i %6i %6i", myEmit->chr[s], 
	       myEmit->loci[s][first], last-first+1, myEmit->loci[s][last]
	       );
      for (int j=0; j< myClone->nLevels; j++){
	fprintf( post_fp, " %.3f", gsl_matrix_get( post, evt, j));
      }
      fprintf( post_fp,"\n");
    }
  }
}

void print_mean_tcn( FILE * mntcn_fp, Clone * myClone, Emission * myEmit, int s, cmdl_opts& opts){
  int myChr = myEmit->chr[s];
  for (int evt=0; evt < myEmit->nEvents[s]; evt++){   
    int first = myEmit->idx_of_event[s][evt];     
    int last  = (evt < myEmit->nEvents[s]-1) ?  myEmit->idx_of_event[s][evt+1]-1 : myEmit->nSites[s]-1; 
    if( opts.print_all || myEmit->coarse_grained == 0 ){
      for (int idx=first; idx<=last; idx++){
      	fprintf( mntcn_fp, "%i %6i", myChr, myEmit->loci[s][idx]);
	for (int t=0; t<myEmit->nTimes; t++){
	  double tcn 
	    = (myEmit->mean_tcn!=NULL) 
	    ? myEmit->mean_tcn[t][s][evt] 
	    : myClone->tcn[myChr][t][myClone->level_of[myChr]];
	  fprintf( mntcn_fp, " %.3f", tcn);
	}
	fprintf( mntcn_fp, "\n");
      }
    }
    else{
      fprintf( mntcn_fp, "%i %6i %6i %6i", myChr, myEmit->loci[s][first], last-first+1, myEmit->loci[s][last]);
      for (int t=0; t<myEmit->nTimes; t++){
	double tcn 
	  = (myEmit->mean_tcn!=NULL) 
	  ? myEmit->mean_tcn[t][s][evt]
	  : myClone->tcn[myChr][t][myClone->level_of[myChr]];
	fprintf( mntcn_fp, " %.3f", tcn);
      }
      fprintf( mntcn_fp, "\n");
    }
  }
}


void print_avail_cn( FILE * avcn_fp, Clone * myClone, Emission * myEmit, int s, cmdl_opts& opts){
  for (int evt=0; evt < myEmit->nEvents[s]; evt++){   
    int first = myEmit->idx_of_event[s][evt];     
    int last  = (evt < myEmit->nEvents[s]-1) ?  myEmit->idx_of_event[s][evt+1]-1 : myEmit->nSites[s]-1; 
    if( opts.print_all || !myEmit->coarse_grained ){
      for (int idx=first; idx<=last; idx++){
      	fprintf( avcn_fp, "%i %6i", myEmit->chr[s], myEmit->loci[s][idx]);
	for (int t=0; t<myEmit->nTimes; t++){
	  for (int cn=0; cn<=myClone->maxtcn; cn++){
	    fprintf( avcn_fp, " %.3f", myEmit->av_cn[t][s][evt][cn]);
	  }
	}
	fprintf( avcn_fp, "\n");
      }
    }
    else{
      fprintf( avcn_fp, "%i %6i %6i %6i", myEmit->chr[s], 
	       myEmit->loci[s][first], last-first+1, myEmit->loci[s][last]
	       );
      for (int t=0; t<myEmit->nTimes; t++){
	for (int cn=0; cn<=myClone->maxtcn; cn++){
	  fprintf( avcn_fp, " %.3f", myEmit->av_cn[t][s][evt][cn]);
	}
      }
      fprintf( avcn_fp, "\n");
    }
  }
}

