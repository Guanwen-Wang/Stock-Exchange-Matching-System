FROM ubuntu:16.04
RUN mkdir /code
RUN apt-get update && apt-get install -y make && apt-get install -y g++
RUN apt-get update && apt-get install -y sudo
RUN apt-get update && apt-get install -y postgresql
RUN apt-get update && apt-get install -y postgresql-contrib
RUN apt-get update && apt-get install -y libpqxx-dev
RUN apt-get update && apt-get install -y libxml++2.6-dev
RUN apt-get update && apt-get install -y libxml++2.6-doc
RUN apt-get update && apt-get install -y libboost-all-dev
RUN apt-get update && apt-get install -y libicu-dev
RUN apt-get update && apt-get install -y libbz2-dev
RUN apt-get update && apt-get install -y aptitude

WORKDIR /code
ADD . /code