# DRMAA Web Service Proxy

Are you using Grid Engine and want to access using sensible tools instead
of text munging? This web service allows accessing a subset of Grid Engine's
tool chain through the DRMAA interface.

## Installation

You will need:

* Something that provides a DRMAA interface, such as Open Grid Engine
* [Pistach](http://pistache.io/)
* [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) which depends on [SQLite](https://www.sqlite.org/index.html)
* [JsonCpp](https://github.com/open-source-parsers/jsoncpp)
* A C++ compiler that supports C++11

After installing the above, find the directory containing `libdrmaa.so` in your
Grid Engine install.

    find /opt/ogs2011.11 -name 'libdrmaa.so'

Let's suppose that path is `/opt/ogs2011.11/lib/linux-x64`. Now, execute:

    make DRMAA_DIR=/opt/ogs2011.11/lib/linux-x64

This will produce the executable `drmaaws`, which you can stick anywhere you
like and execute to run on port 9080.

## Using DRMAAWS

The web service attempts to provide a stateless interface for accessing DRMAA.
This means if you attempt to execute exactly the same command again, it will
check the status of that command rather than re-executing it.

Launch the DRMAAWS as the user you want to execute:

     DRMAA_PSK=password ./drmaaws

The `DRMAA_PSK` is a pre-shared key between client and server that allows
authorization using signed requests. Each request should include the header:

     Authorization: signed sha1sum

Where sha1sum is the SHA1 sum of the PSK and the request body. This is
vulnerable to replay attack, but since the system is idempotent, this is not a
problem.

Try out this sleep command:

    echo -n '{"drmaa_remote_command":"/bin/sleep", "drmaa_v_argv":["1m"]}' > test.data
    SIG="$( (echo -n $DRMAA_PSK; cat test.data) | tr -d '\n' | sha1sum | cut -f 1 -d " ")"
    curl -i -H "Content-Type: application/json" -H "Authorization: signed ${SIG}" -X POST -d @test.data http://localhost:9080/run

The allowed parameters are:

 * `drmaa_block_email`
 * `drmaa_deadline_time`
 * `drmaa_duration_hlimit`
 * `drmaa_duration_slimit`
 * `drmaa_error_path`
 * `drmaa_input_path`
 * `drmaa_job_category`
 * `drmaa_job_name`
 * `drmaa_join_files`
 * `drmaa_js_state`
 * `drmaa_native_specification`
 * `drmaa_output_path`
 * `drmaa_remote_command`
 * `drmaa_start_time`
 * `drmaa_transfer_files`
 * `drmaa_v_argv`
 * `drmaa_v_email`
 * `drmaa_v_env`
 * `drmaa_wct_hlimit`
 * `drmaa_wct_slimit`
 * `drmaa_wd`

Exactly what these do, is between you and your DRMAA provider.

The web service maintains state in the working directory using a SQLite
database named `drmaaws.db3`. If the system is restarted, it will recover its
state from the database.
