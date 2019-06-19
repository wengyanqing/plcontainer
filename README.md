## PL/Container

This is an implementation of trusted language execution engine capable of
bringing up Docker containers to isolate executors from the host OS, i.e.
implement sandboxing.

The architecture of PL/Container is described at [PL/Container-Architecture](https://github.com/greenplum-db/plcontainer/wiki/PLContainer-Architecture) It's a Greeplum version, but simialr to Postgres.

### Requirements

1. PL/Container runs on CentOS/RHEL 7.x or CentOS/RHEL 6.6+
1. PL/Container requires Docker version 17.05 for CentOS/RHEL 7.x and Docker version 1.7 CentOS/RHEL 6.6+
1. Postgres master branch supported

### Building PL/Container

Get the code repo
```shell
git clone https://github.com/greenplum-db/plcontainer.git
cd plcontainer
git pull origin postgres
```

You can build PL/Container in the following way:

1. Go to the PL/Container directory: `cd plcontainer`
1. Make and install it: `PLC_PG=yes make install`



### Configuring PL/Container

To configure PL/Container environment, you need to enable PL/Container for specific databases by running 
   ```shell
   psql -d your_database -c 'create extension plcontainer;'
   ```

### Unsupported feature
There some features PLContainer doesn't support. For unsupported feature list and their corresponding issue, 
please refer to [Unsupported Feature](https://github.com/greenplum-db/plcontainer/wiki/PLContainer-Unsupported-Features)

### Design

The idea of PL/Container is to use containers to run user defined functions. The current implementation assume the PL function definition to have the following structure:

```sql
CREATE FUNCTION dummyPython() RETURNS text AS $$
# container: plc_python_shared
return 'hello from Python'
$$ LANGUAGE plcontainer;
```

There are a couple of things you need to pay attention to:

1. The `LANGUAGE` argument to Greenplum is `plcontainer`

1. The function definition starts with the line `# container: plc_python_shared` which defines the name of runtime that will be used for running this function. 


### Contributing
PL/Container is maintained by a core team of developers with commit rights to the [plcontainer repository](https://github.com/greenplum-db/plcontainer) on GitHub. At the same time, we are very eager to receive contributions and any discussions about it from anybody in the wider community.
