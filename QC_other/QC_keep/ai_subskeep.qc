void (entity destEnt, float trackSpeed) ai_track = {
   local vector dir;

   dir = (destEnt.origin + destEnt.view_ofs);
   dir = normalize ((dir - self.origin));
   self.velocity = (dir * trackSpeed);
};