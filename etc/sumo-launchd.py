#!/usr/bin/env python
 
#
# sumo-launchd.py -- SUMO launcher daemon for use with TraCI clients
# Copyright (C) 2009 Christoph Sommer <christoph.sommer@informatik.uni-erlangen.de>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
 
"""
For each incoming TCP connection the daemon receives a launch configuration.
It starts SUMO accordingly, then proxies all TraCI Messages.
 
The launch configuration must be sent in the very first TraCI message.
This message must contain a single command, CMD_FILE_SEND and be used to
send a file named "sumo-launchd.launch.xml", which has the following
structure:
 
<?xml version="1.0"?>
<launch>
<basedir path="/home/sommer/src/inet/examples/erlangen6" />
<copy file="net.net.xml" />
<copy file="routes.rou.xml" />
<copy file="sumo.sumo.cfg" type="config" />
</launch>
"""
 
import os
import sys
import tempfile
import shutil
import socket
import struct
import subprocess
import time
import signal
import exceptions
import thread
import xml.dom.minidom
import select
import logging
from optparse import OptionParser
 
 
_CMD_FILE_SEND = 0x75
class UnusedPortLock:
    lock = thread.allocate_lock()

    def __init__(self):
        self.acquired = False

    def __enter__(self):
        self.acquire()

    def __exit__(self):
        self.release()

    def acquire(self):
        if not self.acquired:
            logging.debug("Claiming lock on port")
            UnusedPortLock.lock.acquire()
            self.acquired = True

    def release(self):
        if self.acquired:
            logging.debug("Releasing lock on port")
            UnusedPortLock.lock.release()
            self.acquired = False

def find_unused_port():
    """
Return an unused port number.
"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    sock.bind(('127.0.0.1', 0))
    sock.listen(socket.SOMAXCONN)
    ipaddr, port = sock.getsockname()
    sock.close()
    return port
 
 
def forward_connection(client_socket, server_socket, process):
    """
    Proxy connections until either socket runs out of data or process terminates.
    """

    logging.debug("Starting proxy mode")
 
    do_exit = False
    while not do_exit:
 
        (r, w, e) = select.select([client_socket, server_socket], [], [client_socket, server_socket], 0)
        if client_socket in e:
            do_exit = True
            break
        if server_socket in e:
            do_exit = True
            break
        if client_socket in r:
            try:
                data = client_socket.recv(65535)
                if data == "":
                    do_exit = True
            except:
                do_exit = True
            finally:
                server_socket.send(data)
        if server_socket in r:
            try:
                data = server_socket.recv(65535)
                if data == "":
                    do_exit = True
            except:
                do_exit = True
            finally:
                client_socket.send(data)
        rc = process.poll()
        if (rc != None):
            do_exit = True
            break
        time.sleep(0.1)
    logging.debug("Done with proxy mode")
def parse_launch_configuration(launch_xml_string):
    """
    Returns tuple of options set in launch configuration
    """
    
    p = xml.dom.minidom.parseString(launch_xml_string)
    
    # get root node "launch"
    launch_node = p.documentElement
    if (launch_node.tagName != "launch"):
        raise RuntimeError("launch config root element not <launch>, but <%s>" % launch_node.tagName)
 
    # get "launch.basedir"
    basedir = ""
    basedir_nodes = [x for x in launch_node.getElementsByTagName("basedir") if x.parentNode==launch_node]
    if len(basedir_nodes) > 1:
        raise RuntimeError('launch config contains %d <basedir> nodes, expected at most 1' % (len(basedir_nodes)))
    elif len(basedir_nodes) == 1:
        basedir = basedir_nodes[0].getAttribute("path")
 
    logging.debug("Base dir is %s" % basedir)
 
    # get list of "launch.copy" entries
    copy_nodes = [x for x in launch_node.getElementsByTagName("copy") if x.parentNode==launch_node]
    
    return (basedir, copy_nodes)

def run_sumo(runpath, sumo_command, config_file_name, remote_port, client_socket, unused_port_lock):
    """
    Actually run SUMO.
    """

    # create log files
    sumoLogOut = open(os.path.join(runpath, 'sumo-launchd.out.log'), 'w')
    sumoLogErr = open(os.path.join(runpath, 'sumo-launchd.err.log'), 'w')
 
    # start SUMO
    sumo_start = int(time.time())
    sumo_end = None
    sumo_returncode = -1
    sumo_status = None
    try:
        cmd = [sumo_command, "-c", config_file_name]
        logging.info("Starting SUMO (%s) on port %d" % (" ".join(cmd), remote_port))
        sumo = subprocess.Popen(cmd, cwd=runpath, stdin=None, stdout=sumoLogOut, stderr=sumoLogErr, close_fds=True)
 
        sumo_socket = None
 
        connected = False
        tries = 1
        while not connected:
            try:
                logging.debug("Connecting to SUMO (%s) on port %d (try %d)" % (" ".join(cmd), remote_port, tries))
                sumo_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sumo_socket.connect(('127.0.0.1', remote_port))
                break
            except socket.error, e:
                logging.debug("Error (%s)" % e)
                if tries >= 10:
                    raise
                time.sleep(tries * 0.25)
                tries += 1
        unused_port_lock.release()
        forward_connection(client_socket, sumo_socket, sumo)
 
        client_socket.close()
        sumo_socket.close()
 
        logging.debug("Done with proxy mode, killing SUMO")
 
        thread.start_new_thread(subprocess.Popen.wait, (sumo, ))
        time.sleep(0.5)
        if sumo.returncode == None:
            logging.debug("SIGTERM")
            os.kill(sumo.pid, signal.SIGTERM)
            time.sleep(0.5)
            if sumo.returncode == None:
                logging.debug("SIGKILL")
                os.kill(sumo.pid, signal.SIGKILL)
                time.sleep(1)
                if sumo.returncode == None:
                    logging.debug("Warning: SUMO still not dead. Waiting 10 more seconds...")
                    time.sleep(10)
 
        logging.info("Done running SUMO")
        sumo_returncode = sumo.returncode
        if sumo_returncode == 0:
            sumo_status = "Done."
        elif sumo_returncode != None:
            sumo_status = "Exited with error code %d" % sumo_returncode
        else:
            sumo_returncode = -1
            sumo_status = "Undef"
 
    except OSError, e:
        sumo_status = "Execution failed (%s)" % e
 
    except exceptions.SystemExit:
        sumo_status = "Premature launch script exit"
 
    except exceptions.KeyboardInterrupt:
        sumo_status = "Keyboard interrupt."
 
    except:
        raise
    
    # statistics
    sumo_end = int(time.time())
 
    # close log files
    sumoLogOut.close()
    sumoLogErr.close()
 
    # read log files
    sumoLogOut = open(os.path.join(runpath, 'sumo-launchd.out.log'), 'r')
    sumoLogErr = open(os.path.join(runpath, 'sumo-launchd.err.log'), 'r')
    sumo_stdout = sumoLogOut.read()
    sumo_stderr = sumoLogErr.read()
    sumoLogOut.close()
    sumoLogErr.close()
 
    # prepare result XML
    CDATA_START = '<![CDATA['
    CDATA_END = ']]>'
    result_xml = '<?xml version="1.0"?>\n'
    result_xml += '<status>\n'
    result_xml += '\t<%s>%s</%s>\n' % ("exit-code", sumo_returncode, "exit-code")
    if sumo_start:
        result_xml += '\t<%s>%s</%s>\n' % ("start", sumo_start, "start")
    if sumo_end:
        result_xml += '\t<%s>%s</%s>\n' % ("end", sumo_end, "end")
    if sumo_status:
        result_xml += '\t<%s>%s</%s>\n' % ("status", sumo_status, "status")
    result_xml += '\t<%s>%s</%s>\n' % ("stdout", CDATA_START + sumo_stdout.replace(CDATA_END, CDATA_END + CDATA_END + CDATA_START) + CDATA_END, "stdout")
    result_xml += '\t<%s>%s</%s>\n' % ("stderr", CDATA_START + sumo_stderr.replace(CDATA_END, CDATA_END + CDATA_END + CDATA_START) + CDATA_END, "stderr")
    result_xml += '</status>\n'
 
    return result_xml
 
 
def copy_and_modify_files(basedir, copy_nodes, runpath, remote_port):
    """
Copy (and modify) files, return config file name
"""
    
    config_file_name = None
    for copy_node in copy_nodes:
 
        file_src_name = None
        file_dst_name = None
        file_contents = None
 
        # Read from disk?
        if copy_node.hasAttribute("file"):
            file_src_name = copy_node.getAttribute("file")
            file_src_path = os.path.join(basedir, file_src_name)
 
            # Sanity check
            if file_src_name.find("/") != -1:
                raise RuntimeError('name of file to copy "%s" contains a "/"' % file_src_name)
            if not os.path.exists(file_src_path):
                raise RuntimeError('file "%s" does not exist' % file_src_path)
 
            # Read contents
            file_handle = open(file_src_path, 'rb')
            file_contents = file_handle.read()
            file_handle.close()
 
        # By now we need a destination name and contents
        if copy_node.hasAttribute("name"):
            file_dst_name = copy_node.getAttribute("name")
        elif file_src_name:
            file_dst_name = file_src_name
        else:
            raise RuntimeError('<copy> node with no destination name: %s' % copy_node.toxml())
        if file_contents == None:
            raise RuntimeError('<copy> node with no contents: %s' % copy_node.toxml())
 
        # Needs to be parsed?
        if copy_node.getAttribute("type") == "config":
            config_file_name = file_dst_name
 
            config_parser = xml.dom.minidom.parseString(file_contents)
            config_xml = config_parser.documentElement
 
            # get or create "launch.config.**.remote-port"
            remote_port_nodes = config_xml.getElementsByTagName("remote-port")
            if len(remote_port_nodes) > 1:
                raise RuntimeError('config file "%s" contains %d <remote-port> nodes, expected at most 1' % (file_dst_name, len(remote_port_nodes)))
            elif len(remote_port_nodes) < 1:
                remote_port_node = config_parser.createElement("remote-port")
                remote_port_node.appendChild(config_parser.createTextNode(str(remote_port)))
                config_xml.appendChild(remote_port_node)
            else:
                remote_port_node = remote_port_nodes[0]
                for n in remote_port_node.childNodes:
                    remote_port_node.removeChild(n)
                remote_port_node.appendChild(config_parser.createTextNode(str(remote_port)))
 
            file_contents = config_xml.toxml()
 
        # Write file into rundir
        file_dst_path = os.path.join(runpath, file_dst_name)
        file_handle = open(file_dst_path, "wb")
        file_handle.write(file_contents)
        file_handle.close()
 
    # make sure that we copied a config file
    if not config_file_name:
        raise RuntimeError('launch config contained no <copy> node with type="config"')
 
    return config_file_name
 
 
def handle_launch_configuration(sumo_command, launch_xml_string, client_socket):
    """
    Process launch configuration in launch_xml_string.
    """

    # create temporary directory
    logging.debug("Creating temporary directory...")
    runpath = tempfile.mkdtemp(prefix="sumo-launchd-tmp-")
    if not runpath:
        raise RuntimeError("Could not create temporary directory")
    if not os.path.exists(runpath):
        raise RuntimeError('Temporary directory "%s" does not exist, even though it should have been created' % runpath)
    logging.debug("Temporary dir is %s" % runpath)

    result_xml = None
    unused_port_lock = UnusedPortLock()
    try:    
        # parse launch configuration 
        (basedir, copy_nodes) = parse_launch_configuration(launch_xml_string)

        # find remote_port
        logging.debug("Finding free port number...")
        unused_port_lock.__enter__()
        remote_port = find_unused_port()
        logging.debug("...found port %d" % remote_port)

        # copy (and modify) files
        config_file_name = copy_and_modify_files(basedir, copy_nodes, runpath, remote_port)
        
        # run SUMO
        result_xml = run_sumo(runpath, sumo_command, config_file_name, remote_port, client_socket, unused_port_lock)

    finally:
        unused_port_lock.__exit__()

        # clean up
        logging.debug("Cleaning up")
        shutil.rmtree(runpath)

        logging.debug('Result: "%s"' % result_xml)

    return result_xml
 
 
def read_launch_config(conn):
    """
Read (and return) launch configuration from socket
"""
 
    # Get TraCI message length
    msg_len_buf = ""
    while len(msg_len_buf) < 4:
        msg_len_buf += conn.recv(4 - len(msg_len_buf))
    msg_len = struct.unpack("!i", msg_len_buf)[0] - 4
 
    logging.debug("Got TraCI message of length %d" % msg_len)
 
    # Get TraCI command length
    cmd_len_buf = ""
    cmd_len_buf += conn.recv(1)
    cmd_len = struct.unpack("!B", cmd_len_buf)[0] - 1
    if cmd_len == -1:
        cmd_len_buf = ""
        while len(cmd_len_buf) < 4:
            cmd_len_buf += conn.recv(4 - len(cmd_len_buf))
        cmd_len = struct.unpack("!i", cmd_len_buf)[0] - 5
 
    logging.debug("Got TraCI command of length %d" % cmd_len)
 
    # Get TraCI command ID
    cmd_id_buf = ""
    cmd_id_buf += conn.recv(1)
    cmd_id = struct.unpack("!B", cmd_id_buf)[0]
    if cmd_id != _CMD_FILE_SEND:
        raise RuntimeError("Expected CMD_FILE_SEND (0x%x), but got 0x%x" % (_CMD_FILE_SEND, cmd_id))
 
    logging.debug("Got TraCI command 0x%x" % cmd_id)
 
    # Get File name
    fname_len_buf = ""
    while len(fname_len_buf) < 4:
        fname_len_buf += conn.recv(4 - len(fname_len_buf))
    fname_len = struct.unpack("!i", fname_len_buf)[0]
    fname = conn.recv(fname_len)
    if fname != "sumo-launchd.launch.xml":
        raise RuntimeError('Launch configuration must be named "sumo-launchd.launch.xml", got "%s" instead.' % fname)
 
    logging.debug('Got CMD_FILE_SEND for "%s"' % fname)
 
    # Get File contents
    data_len_buf = ""
    while len(data_len_buf) < 4:
        data_len_buf += conn.recv(4 - len(data_len_buf))
    data_len = struct.unpack("!i", data_len_buf)[0]
    data = conn.recv(data_len)
 
    logging.debug('Got CMD_FILE_SEND with data "%s"' % data)
 
    # Send OK response
    response = struct.pack("!iBBBi", 4+1+1+1+4, 1+1+1+4, _CMD_FILE_SEND, 0x00, 0x00)
    conn.send(response)
    
    return data
        
        
def handle_connection(sumo_command, conn, addr):
    """
Handle incoming connection.
"""
 
    logging.debug("Handling connection from %s on port %d" % addr)
 
    try:
        data = read_launch_config(conn)
        handle_launch_configuration(sumo_command, data, conn)
    except Exception, e:
        logging.error("Aborting on error: %s" % e)
    
    finally:
        logging.debug("Closing connection from %s on port %d" % addr)
        conn.close()
 
 
def wait_for_connections(sumo_command, sumo_port, bind_address):
    """
Open TCP socket, wait for connections, call handle_connection for each
"""
    
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind((bind_address, sumo_port))
    listener.listen(5)
    logging.info("Listening on port %d" % sumo_port)
    try:
        while True:
            conn, addr = listener.accept()
            logging.debug("Connection from %s on port %d" % addr)
            thread.start_new_thread(handle_connection, (sumo_command, conn, addr))
    
    except exceptions.SystemExit:
        logging.warning("Killed.")
    
    except exceptions.KeyboardInterrupt:
        logging.warning("Keyboard interrupt.")
    
    except:
        raise
    
    finally:
        # clean up
        logging.info("Shutting down.")
        listener.close()
 
    
def main():
    """
Program entry point when run interactively.
"""
 
    # Option handling
    parser = OptionParser()
    parser.add_option("-c", "--command", dest="command", default="sumo", help="run SUMO as COMMAND [default: %default]", metavar="COMMAND")
    parser.add_option("-p", "--port", dest="port", type="int", default=9999, action="store", help="listen for connections on PORT [default: %default]", metavar="PORT")
    parser.add_option("-b", "--bind", dest="bind", default="127.0.0.1", help="bind to ADDRESS [default: %default]", metavar="ADDRESS")
    parser.add_option("-L", "--logfile", dest="logfile", default="sumo-launchd.log", help="log messages to LOGFILE [default: TMPDIR/%default]", metavar="LOGFILE")
    parser.add_option("-v", "--verbose", dest="count_verbose", default=0, action="count", help="increase verbosity [default: don't log infos, debug]")
    parser.add_option("-q", "--quiet", dest="count_quiet", default=0, action="count", help="decrease verbosity [default: log warnings, errors]")
    (options, args) = parser.parse_args()
    _LOGLEVELS = (logging.ERROR, logging.WARN, logging.INFO, logging.DEBUG)
    loglevel = _LOGLEVELS[max(0, min(1 + options.count_verbose - options.count_quiet, len(_LOGLEVELS)-1))]
 
    # catch SIGTERM to exit cleanly when we're kill-ed
    signal.signal(signal.SIGTERM, lambda signum, stack_frame: sys.exit(1))
    
    # configure logging
    logging.basicConfig(filename=os.path.join(tempfile.gettempdir(), options.logfile), level=loglevel)
 
    # this is where we'll spend our time
    wait_for_connections(options.command, options.port, options.bind)
 
 
# Start main() when run interactively
if __name__ == '__main__':
    main()
 
 
 
 
 
 
