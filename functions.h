// convert AIS "minutes with 4 digits precision" to degree
float m4tofloat(long min4) {
  long intPart = min4 / 60L;
  long fracPart = intPart % 10000L;
  if (fracPart < 0)
      fracPart = -fracPart;
  char frac[6];
  sprintf(frac, "%04ld", fracPart);
  String degstr = String(intPart/10000L) + "." + String(frac);
  float deg = degstr.toFloat();
  return deg;
}

//convert degrees to radians
double dtor(double fdegrees)
{
return(fdegrees * PI / 180);
}

//Convert radians to degrees
double rtod(double fradians)
{
return(fradians * 180.0 / PI);
}

//Calculate distance form lat1/lon1 to lat2/lon2 using haversine formula
//Note lat1/lon1/lat2/lon2 must be in radians
//Returns distance in nm
float CalcDistance(double lat1, double lon1, double lat2, double lon2)
{
  double dlon, dlat, a, c;
  double dist = 0.0;
  dlon = dtor(lon2 - lon1);
  dlat = dtor(lat2 - lat1);
  //Serial.println(dlat, 5);
  a = pow(sin(dlat/2),2) + cos(dtor(lat1)) * cos(dtor(lat2)) * pow(sin(dlon/2),2);
  c = 2 * atan2(sqrt(a), sqrt(1-a));
  dist = 3443.93089 * c;  //radius of the earth (6378140 meters) in nautical miles 3443.93089
  return((float)dist);
}

//Calculate bearing from lat1/lon1 to lat2/lon2
//Note lat1/lon1/lat2/lon2 must be in radians
//Returns bearing in degrees
int CalcBearing(double lat1, double lon1, double lat2, double lon2)
{
  lat1 = dtor(lat1);
  lon1 = dtor(lon1);
  lat2 = dtor(lat2);
  lon2 = dtor(lon2);
  
  //determine angle
  double bearing = atan2(sin(lon2-lon1)*cos(lat2), (cos(lat1)*sin(lat2))-(sin(lat1)*cos(lat2)*cos(lon2-lon1)));
  //convert to degrees
  bearing = rtod(bearing);
  //use mod to turn -90 = 270
  bearing = fmod((bearing + 360.0), 360);
  return (int)(bearing + 0.5);
}

void ComputeDestPoint(double lat1, double lon1, int iBear, int iDist, double *lat2, double *lon2)
{
  double bearing = dtor((double) iBear);
  double dist = (double) iDist / 3443.93089;
  lat1 = dtor(lat1);
  lon1 = dtor(lon1);
  *lat2 = asin(sin(lat1)* cos(dist)+ cos(lat1)* sin(dist)*cos(bearing));
  *lon2 = lon1 + atan2(sin(bearing)*sin(dist)*cos(lat1), cos(dist)-sin(lat1)*sin(*lat2));
  *lon2 = fmod( *lon2 + 3 * PI, 2*PI )- PI;
  *lon2 = rtod( *lon2);
  *lat2 = rtod( *lat2);
}


String getShiptype(uint8_t st) {
    String Shiptype; 
    if (st == 0) {
      Shiptype = "";
    } else if(st >= 20 && st <=29) {
      Shiptype = shiptype[0];            
    } else if (st == 51) {
      Shiptype = shiptype[1];      
    } else if (st == 31 || st == 32 || st == 52) {
      Shiptype = shiptype[2];      
    } else if (st == 30) {
      Shiptype = shiptype[3];      
    } else if (st == 36) {
      Shiptype = shiptype[4];      
    } else if (st == 37) {
      Shiptype = shiptype[5];    
    } else if (st == 34) {
      Shiptype = shiptype[6];      
    } else if (st == 35) {
      Shiptype = shiptype[7]; 
    } else if (st == 33) {
      Shiptype = shiptype[8];       
    } else if (st == 50) {
      Shiptype = shiptype[9];      
    } else if (st == 54 || st == 55) {
      Shiptype = shiptype[10];
    } else if (st == 58) {
      Shiptype = shiptype[11];      
    } else if (st == 53) {
      Shiptype = shiptype[12];                  
    } else if (st >= 40 && st <= 49) {
      Shiptype = shiptype[13];   
    } else if (st ==  56 || st == 57 || st == 59) {
      Shiptype = shiptype[14];    
    } else if (st >= 60 && st <= 69) {
      Shiptype = shiptype[15];      
    } else if (st >= 70 && st <= 79) {
      Shiptype = shiptype[16];      
    } else if (st >= 80 && st <= 89) {
      Shiptype = shiptype[17];      
    } else {
      Shiptype = shiptype[18]; 
    }
    return Shiptype;  
}

// gets a part of a string devided by separators. like unix "cut"
String getValue(String data, char separator, int index)
{
  uint8_t found = 0;
  int8_t strIndex[] = {0, -1};
  uint8_t maxIndex = data.length()-1;

  for(uint8_t i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}


void draw_track(float distance, uint16_t bearing, float sog, uint16_t cog, uint16_t color, String num)
{
  // scale calculations
  // radar area: -4 miles to +4 miles = 8 miles
  // screen area: -160 px to +160 px = 320 px
  // 1 nm -> 40 px
  // length/end of track -> estimated position in 10 minutes
  // 1 kn -> 6.667 px
  
  float d_x = sin (dtor(bearing)) * distance * 40;
  float d_y = cos (dtor(bearing)) * distance * 40;

  float tiltAngleRad = cog * DEG_TO_RAD; // convert angle to radians
  int xEnd = CENTER_X  + d_x + (sog * 6.667) * sin (tiltAngleRad); // Ending x-coordinate offset & radius
  int yEnd = CENTER_Y  - d_y - (sog * 6.667) * cos (tiltAngleRad); // Ending y-coordinate offset & radius
  spr.drawLine (CENTER_X  + d_x, CENTER_Y - d_y, xEnd, yEnd, color);

  spr.setTextColor(color,TFT_BLACK);
  spr.drawCentreString(num, CENTER_X + d_x + 1, CENTER_Y - d_y - 3, 1);
}


void unpackGPS(String mnea_gps){
  //Serial.println(mnea_gps);

  // we only like RMC messages
  String tmp_str = getValue(mnea_gps,',',0);
  if (tmp_str != "$GPRMC") return;
  
  // UTC time
  tmp_str = getValue(mnea_gps,',',1); // get first field in str
  //Serial.println(tmp_str);
  uint8_t Hour = tmp_str.substring(0, 2).toInt();
  uint8_t Minute = tmp_str.substring(2, 4).toInt();
  uint8_t Second = tmp_str.substring(4, 6).toInt();
  //Serial.println(Second);
  float tmp_fl = tmp_str.toFloat();
  unsigned int utc = ((int)tmp_fl+70)/100; //+ 0.5;
  //Serial.println(utc);
  char strutc[5];
  sprintf (strutc, "%04d", utc); 
  utc_str = (String)strutc[0]+(String)strutc[1]+":"+(String)strutc[2]+(String)strutc[3];

  // date
  tmp_str = getValue(mnea_gps,',',9);
  String Day = tmp_str.substring(0, 2);
  String Month = tmp_str.substring(2, 4);
  String Year = tmp_str.substring(4);
  date_str = Year + "-" + Month + "-" + Day;
  unsigned int date = Year.toInt() * 10000 + Month.toInt() * 100 + Day.toInt();
  //Serial.println(date);
  stamp.setDateTime(("20" + Year).toInt(), Month.toInt(), Day.toInt(), Hour, Minute, Second);
  unixTime = stamp.getUnix();
  //Serial.println(unixTime);

  // Fix
  tmp_str = getValue(mnea_gps,',',2);
  fix = (tmp_str == "A")?true:false;
  
  // Latitude
  tmp_str = getValue(mnea_gps,',',3);
  //Serial.println(tmp_str);
  String lat_degree_str = tmp_str.substring(0, 2);
  unsigned int lat_degree = lat_degree_str.toInt();
  //Serial.println(lat_degree_str + "D");
  String min_str = tmp_str.substring(2);
  //Serial.println(min_str + "M");
  float fl_lat_min = min_str.toFloat();
  //Serial.println(fl_lat_min, 5);
  char ch_lat_min[5];
  sprintf(ch_lat_min, "%05.2f", fl_lat_min);
  String lat_min_str(ch_lat_min);    
  String lat_dir = getValue(mnea_gps,',',4);
  lat_dec = lat_degree + (fl_lat_min / 60);
  if (lat_dir == "S") lat_dec = lat_dec * (-1);
  //Serial.println(lat_dec, 5);

  // Longitude
  tmp_str = getValue(mnea_gps,',',5);
  //Serial.println(tmp_str);
  String lon_degree_str = tmp_str.substring(0, 3);
  unsigned int lon_degree = lon_degree_str.toInt();
  //Serial.println(lon_degree_str+"D");
  min_str = tmp_str.substring(3);
  //Serial.println(min_str+"M");
  float fl_lon_min = min_str.toFloat();
  char ch_lon_min[5];
  sprintf (ch_lon_min, "%05.2f", fl_lon_min);
  String lon_min_str(ch_lon_min);
  //Serial.print(ch_lon_min);
  String lon_dir = getValue(mnea_gps,',',6);
  //Serial.println(lon_dir);
  lon_dec = lon_degree + (fl_lon_min / 60);
  if (lon_dir == "W") lon_dec = lon_dec * (-1);
  //Serial.println(lon_dec, 5);

  // Speed
  tmp_str = getValue(mnea_gps,',',7);
  tmp_fl = tmp_str.toFloat();
  fl_speed = tmp_fl;
  char ch_speed[3];
  sprintf (ch_speed, "%3.1f", fl_speed);
  String speed_str(ch_speed);
  //Serial.println(ch_speed);

  // Track
  tmp_str = getValue(mnea_gps,',',8);
  tmp_fl = tmp_str.toFloat();
  track = tmp_fl;
  int int_track = (int)(track + 0.5);
  char ch_track[3];
  sprintf (ch_track, "%003d", int_track);
  String track_str(ch_track);
  //Serial.println(track_str);

  // me
  me_line1 = "  " + lat_degree_str + "` " + lat_min_str + "' " + lat_dir + "    " + track_str +"`    ";
  me_line2 = " " + lon_degree_str + "` " + lon_min_str + "' " + lon_dir + "  " + speed_str +" kn   ";
  //Serial.println(me_line1);

  neighbor[0] = { // entry for center of screen | refreshed on every GPS message (once a second). This is why it's living here.
    111111111,
    lat_dec,
    lon_dec,
    track,
    fl_speed,
    "MYSELF",
    "MYSIGN",
    unixTime,
    5,
    "Sailing"
  };
}     // end unpackGPS()

void unpackAIS(char aischar[]) {
  freeFound = false;
  uint8_t idx;
  AIS ais(aischar);
  // mmsi
  unsigned long mmsi = ais.get_mmsi();
  // latitude
  float lati = m4tofloat(ais.get_latitude());
  // longitude
  float longi = m4tofloat(ais.get_longitude());
  // cog
  float cog = float(ais.get_COG());
  cog = cog / 10;
  // sog
  float sog = float(ais.get_SOG());
  sog = sog / 10;
  // ship name
  String name = ais.get_shipname();
  // callsign
  String callsign = ais.get_callsign();
  // lastseen
  uint32_t lastseen = unixTime;
  // length
  uint16_t length = ais.get_to_bow() + ais.get_to_stern();
  // ship/mission type
  String type = getShiptype(ais.get_shiptype());
  // distance from me
  float distance = CalcDistance(lat_dec, lon_dec, lati, longi);
  
  /*
  Serial.println("<---");
  Serial.println(mmsi);
  Serial.print("cog: ");Serial.println(cog);
  Serial.print("sog: ");Serial.println(sog,3);
  Serial.print("lat: ");Serial.print(lati,3);Serial.print(" - ");Serial.println(lat_dec,3);
  Serial.print("lon: ");Serial.print(longi,3);Serial.print(" - ");Serial.println(lon_dec,3);
  Serial.print("<");Serial.print(name);Serial.println(">");
  Serial.print("call: ");Serial.println(callsign);
  Serial.print("len: ");Serial.println(length);
  Serial.print("last: ");Serial.println(lastseen);
  Serial.print("type: ");Serial.println(type);
  Serial.print("dist: ");Serial.println(distance);
  Serial.println("--->");
  Serial.println();
  */

  //  danger of collision while DB is full
  if ((distance < COL_DIST) && (dbfull)) {
    float furthestDist = 0.0;
    uint8_t furthest;
    for (uint8_t i=1; i < DBSIZE; i++){
      if (CalcDistance(lat_dec, lon_dec, neighbor[i].lati, neighbor[i].longi) > furthestDist) {
        furthestDist = CalcDistance(lat_dec, lon_dec, neighbor[i].lati, neighbor[i].longi);
        furthest = i;
      }      
    }
    // delete furthest dataset  or better oldest?
    neighbor[furthest] = {};
    nextFree = furthest;
    dbfull = false;
  }
  
  // mmsi already known? Get the index. Else create new.
  for (uint8_t i = 0; i < DBSIZE ; i++) {
    if (neighbor[i].mmsi == mmsi) {
      idx = i;
      break;
    } else {      
      idx = nextFree;
      if (dbfull) idx = 0xFF;
    }
  }
  Serial.println(nextFree);
  //Serial.println();

  // put AIS values in struct-db
  if (((distance <= 5.0) || (idx != nextFree)) && (idx < DBSIZE)) {
    if (mmsi > 0) neighbor[idx].mmsi = mmsi;
    if (lati != 0) neighbor[idx].lati = lati;
    if (longi != 0) neighbor[idx].longi = longi;
    if (cog > 0) neighbor[idx].cog = cog;
    if (sog > 0) neighbor[idx].sog = sog;
    if (name != "") neighbor[idx].name = name;
    if (callsign != "") neighbor[idx].callsign = callsign;
    neighbor[idx].lastseen = lastseen;
    if (length > 0) neighbor[idx].length = length;
    if (type != "") neighbor[idx].type = type;
  }  
} //  end unpackAIS()


// delete old (older than 6 min [360 sec] ) or far away (more than 5 miles) entries in struct-db
void clearUp() {
  for (uint8_t i=1; i < DBSIZE; i++) {
    float distance = CalcDistance(lat_dec, lon_dec, neighbor[i].lati, neighbor[i].longi);
    if ((neighbor[i].lastseen < (unixTime - 360)) || (distance > 5.0)) {
      neighbor[i] = {};
      freeFound = false;
    }
      memset(neighborline1, 0, sizeof(neighborline1));
      memset(neighborline2, 0, sizeof(neighborline2));
      memset(neighborcol, 0, sizeof(neighborcol));
      memset(neighborbgcol, 0, sizeof(neighborbgcol));
  }
  //tft.init();
}

void messageMgr() {
  // check if ships are very close ( < 0.2 miles)
  for (uint8_t i=1; i < DBSIZE; i++){
    if ((lat_dec != 0) && (CalcDistance(lat_dec, lon_dec, neighbor[i].lati, neighbor[i].longi) < COL_DIST)) {
      collision = true;
      break;
    } else {
      collision = false;
    }  
  }
  alarmlevel = 0;
  if (!fix) alarmlevel = 1;
  if (dbfull) alarmlevel = 2;
  if (collision) alarmlevel = 3;  
}


void demoData(){
    // Fififehn: 53.109   8.118
    //                 mmsi,   lat,     lon, cog,    sog,        name,callsign,  lastseen,length,shiptype
  neighbor[1] = { 211111111, 53.10,   8.09 , 120,   12.4,    "Sirius","AB4322",1683392235,     5,"Pleasure" }; // 1.04 nm dist // 134° bearing
  neighbor[2] = { 341746110, 53.06,   8.16 ,  76,   20.2,       ""   ,""      ,1683392335,   120,"Tanker" };
  neighbor[5] = { 265547250, 53.16,   8.08 , 270,   30.0,    "Speedy","SPEEDY",1683382235,    20,"Police" };
  neighbor[6] = { 603916439, 53.13,   8.13 , 305,    4.6,          "",      "",1683392435,     0,"other" };
  neighbor[9] = { 992509976, 53.07,   8.09 ,  20,   18.5,    "Frauke","DE4321",1683392835,    13,"Passenger" };
}
