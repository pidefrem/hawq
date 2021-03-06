/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HAWQ_SRC_TEST_FEATURE_LIB_HDFS_CONFIG_H_
#define HAWQ_SRC_TEST_FEATURE_LIB_HDFS_CONFIG_H_

#include <string>
#include <vector>

#include "psql.h"
#include "sql_util.h"
#include "xml_parser.h"
#include "lib/compent_config.h"

namespace hawq {
namespace test {

/**
 * HdfsConfig common libray. Get detailed information about HDFS
 * including checking state of namenodes and datanodes, get parameter value
 * @author Chunling Wang
 */
class HdfsConfig : public CompentConfig {
  public:
    /**
     * HdfsConfig constructor
     */
    HdfsConfig();

    /**
     * HdfsConfig destructor
     */
    ~HdfsConfig() {}

    /**
     * whether HDFS is in HA mode
     * @return true if HDFS is HA, false if HDFS is not HA
     */
    bool isHA() { return isHACluster; }

    /**
     * whether HDFS is kerbos
     * @return 1 if HDFS is configured kerberos, 0 if HDFS is not configured kerberos, -1 if failed to load from HAWQ configuration file
     */
    int isConfigKerberos();

    /**
     * whether HDFS supports truncate operation
     * @return 1 if HDFS supports truncate operation, 0 if HDFS does not support truncate operation
     */
    int isTruncate();

    /**
     * get HADOOP working directory
     * @return HADOOP working directory
     */
    std::string getHadoopHome();

    bool getCluster();
    /**
     * get HDFS namenode's host ('hostname:port' for non-HA, 'servicename' for HA)
     * @ param namenodehost, namenode host reference which will be set
     * @ return true if getNamenodeHost succeeded
     */
    bool getNamenodeHost(std::string &namenodehost);
    
    /**
     * get HDFS active namenode's hostname and port information
     * @param activenamenode, active namenode hostname reference which will be set
     * @param port, active namenode port reference which will be set
     * @return true if getActiveNamenode succeeded
     */
    bool getActiveNamenode(std::string &activenamenode,
                           int &port);

    /**
     * get HDFS standby namenode's hostname and port information
     * @param standbynamenode, standby namenode hostname reference which will be set
     * @param port, standby namenode port reference which will be set
     * @return true if getStandbyNamenode succeeded
     */
    bool getStandbyNamenode(std::string &standbynamenode,
                            int &port);

    /**
     * get HDFS namenode(s) information
     * @param namenodes, namenodes' hostnames reference which will be set
     * @param port, namenodes' ports reference which will be set
     */
    void getNamenodes(std::vector<std::string> &namenodes,
                      std::vector<int> &port);

    bool checkNamenodesHealth();
    /**
     * get HDFS datanodes information
     * @param datanodelist, datanodes' hostnames reference which will be set
     * @param port, datanodes' ports reference which will be set
     */
    void getDatanodelist(std::vector<std::string> &datanodelist,
                         std::vector<int> &port);

    /**
     * get HDFS active datanodes information
     * @param activedatanodes, active datanodes' hostnames reference which will be set
     * @param port, active datanodes' ports reference which will be set
     */
    void getActiveDatanodes(std::vector<std::string> &activedatanodes,
                            std::vector<int> &port);

    /**
     * whether HDFS is in safe mode
     * @return 1 if HDFS is in safe node, 0 if HDFS is in not safe node
     */
    int isSafemode();

    /**
     * get parameter value in ./etc/hdfs-client.xml or ./etc/hadoop/hdfs-site.xml according to parameter name
     * @param parameterName, used to get parameter value
     * @param conftype, get parameter value, 'hdfs' or 'HDFS' from ./etc/hdfs-client.xml, others from ./etc/hadoop/hdfs-site.xml
     * @return parameter value
     */
    std::string getParameterValue(const std::string &parameterName, const std::string &conftype);
    
    /**
     * get parameter value in ./etc/hadoop/hdfs-site.xml according to parameter name
     * @param parameterName, used to get parameter value
     * @return parameter value
     */
    std::string getParameterValue(const std::string &parameterName);

    /**
     * set parameter value in ./etc/hdfs-client.xml or ./etc/hadoop/hdfs-site.xml according to parameter name
     * @param parameterName, parameter name which used to set parameter value
     * @param parameterValue, parameter value which to be set
     * @param conftype, get parameter value, 'hdfs' or 'HDFS' from ./etc/hdfs-client.xml, others from ./etc/hadoop/hdfs-site.xml
     * @return true if succeeded
     */
    bool setParameterValue(const std::string &parameterName, const std::string &parameterValue, const std::string &conftype);

    /**
     * set parameter value in ./etc/hadoop/hdfs-site.xml according to parameter name
     * @param parameterName, parameter name which used to set parameter value
     * @param parameterValue, parameter value which to be set
     * @return true if succeeded
     */
    bool setParameterValue(const std::string &parameterName, const std::string &parameterValue);

    //return active data node number
    int getActiveDatanodesNum();

    /**
     * @return hdfs user
     */
    std::string getHdfsUser();

  protected:
    /**
     * load key-value parameters in ./etc/hdfs-client.xml
     * @return true if succeeded
     */
    bool LoadFromHawqConfigFile();

    /**
     * load key-value parameters in ./etc/hadoop/hdfs-site.xml
     * @return true if succeeded
     */
    bool LoadFromHdfsConfigFile();

    /**
     * get HDFS active or standby namenode information in HA mode according to the namenodetype
     * @param namenodetype, used to specify active or standby namenode information
     * @param namenode, namenode hostname reference which will be set
     * @param port, namenode port reference which will be set
     * @return true if getHANamenode succeeded
     */
    bool getHANamenode(const std::string &namenodetype, std::string &namenode, int &port);

    bool isHACluster;

    std::string hdfsuser;
  private:
    bool __isHA();
  private:
    std::unique_ptr<XmlConfig> hawqxmlconf;
    bool isLoadFromHawqConfigFile;
    std::unique_ptr<XmlConfig> hdfsxmlconf;
    bool isLoadFromHdfsConfigFile;
};

} // namespace test
} // namespace hawq

#endif /* HAWQ_SRC_TEST_FEATURE_LIB_HDFS_CONFIG_H_ */
