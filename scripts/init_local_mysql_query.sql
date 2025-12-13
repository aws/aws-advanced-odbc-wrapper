CREATE DATABASE IF NOT EXISTS test_database;
CREATE USER 'test_username'@'localhost' IDENTIFIED WITH caching_sha2_password BY 'test_password';
GRANT ALL on *.* TO 'test_username'@'localhost';
FLUSH PRIVILEGES;
