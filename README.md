In order to get the knowledge base working on the robot, you need to do the following things:

1. Make sure that MySQL is installed using apt (it didn't seem to work when installed from source), following the instructions here:
https://dev.mysql.com/doc/mysql-apt-repo-quick-guide/en/#apt-repo-fresh-install

2. Next, in order to get the interface to work download MySQL Connector/C++ Version 8 from here:
https://dev.mysql.com/downloads/connector/cpp/
  2.1 Now, you need to tar it: https://dev.mysql.com/doc/connector-cpp/en/connector-cpp-installation-source-distribution.html
  2.2 Build and install the package following the next part of the instructions.
  
3. You will also need MySQL Shell installed. Found here: https://dev.mysql.com/doc/refman/5.7/en/installing-mysql-shell-linux-quick.html
  3.1 Install the X Plugin as detailed here: https://dev.mysql.com/doc/refman/5.7/en/document-store-setting-up.html

4. Run the create_database.sql file in MySQL using 'source /path/to/create_database.sql'. This will create the schema on your SQL server.
  4.1 Update username and password in your program/high_level.cpp to whatever they are for your SQL server.
  
5. Update the makefile and then use 'make' to build the library and your program that uses it. Replace the program listed as 'high_level' with your own program.
  Here you need to insert the proper paths of your MySQL Connector/C++ Library and the path of this repository's include directory on the system. 

6. Run your compiled program that interfaces with the database!