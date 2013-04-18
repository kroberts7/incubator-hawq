package com.emc.greenplum.gpdb.hdfsconnector;

import java.io.IOException;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.mapred.FileSplit;
import org.apache.hadoop.mapred.InputSplit;
import org.apache.hadoop.mapred.JobConf;


/*
 * Fragmenter class for HDFS data resources
 *
 * Given an HDFS data source (a file, directory, or wild card pattern)
 * divide the data into fragments and return a list of them along with
 * a list of host:port locations for each.
 */
public class HdfsDataFragmenter extends BaseDataFragmenter
{	
	private	JobConf jobConf;
	private Log Log;
	
	/*
	 * C'tor
	 */
	public HdfsDataFragmenter(BaseMetaData md) throws IOException
	{
		super(md);
		Log = LogFactory.getLog(HdfsDataFragmenter.class);

		jobConf = new JobConf(new Configuration(), HdfsDataFragmenter.class);
	}
	
	/*
	 * path is a data source URI that can appear as a file 
	 * name, a directory name  or a wildcard returns the data 
	 * fragments in json format
	 */	
	public void GetFragmentInfos(String datapath) throws Exception
	{
		Path	path = new Path("/" + datapath); //yikes! any better way?		

		InputSplit[] splits = getSplits(path);
		
		for (InputSplit split : splits)
		{	
			FileSplit fsp = (FileSplit)split;
			/*
			 * HD-2547: If the file is empty, an empty split is returned:
			 * no locations and no length.
			 */
			if (fsp.getLength() <= 0)
				continue;
			
			String filepath = fsp.getPath().toUri().getPath();
			filepath = filepath.substring(1); // hack - remove the '/' from the beginning - we'll deal with this next 
			FragmentInfo fi = new FragmentInfo(filepath,
					fsp.getLocations());

			fragmentInfos.add(fi);
		}
		
		//print the raw fragment list to log when in debug level
		Log.debug(FragmentInfo.listToString(fragmentInfos, path.toString()));
		
	}
	
	private InputSplit[] getSplits(Path path) throws IOException 
	{
		GPFusionInputFormat fformat = new GPFusionInputFormat();
		GPFusionInputFormat.setInputPaths(jobConf, path);
		return fformat.getSplits(jobConf, 1);
	}
	
}