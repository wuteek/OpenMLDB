import os
import sys
sys.path.append(os.getenv('testpath'))
import libs.conf as conf
from libs.clients.ns_cluster import NsCluster
from libs.clients.tb_cluster import TbCluster


nsc = NsCluster(conf.zk_endpoint, *(i[1] for i in conf.ns_endpoints))
tbc = TbCluster(conf.zk_endpoint, [i[1] for i in conf.tb_endpoints])

nsc.stop_zk()
nsc.kill(*nsc.endpoints)
tbc.kill(*tbc.endpoints)

if len(sys.argv) > 1 and sys.argv[1] == 'teardown':
    pass
else:
    nsc.clear_zk()
    nsc.start_zk()
    nsc.start(*nsc.endpoints)
    tbc.start(tbc.endpoints)
    nsc_leader = nsc.leader