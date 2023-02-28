# License

Uninterruptible media server - UMS is under GNU GPLv2. See COPYING.GPLv2 for
more details.

The project is based on the FFMPEG library with the --enable-gpl flag enabled.
See the ./ffmpeg/compile.sh script to see how FFMPEG library is compiled for
this project.

# Nginx RTMP Docker file

For testing of the system a docker file has been included which is a copy from
[nginx-rtmp-docker](https://github.com/tiangolo/nginx-rtmp-docker) which is
under MIT license. The nginx config from the original project has been adapted
so that the RTMP server closes the connection to receiving clients if no client
is pushing data into the rtmp server.
