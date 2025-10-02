#include <pqxx/pqxx>
#include <iostream>

int main()
{
    try
    {
        pqxx::connection conn{ "dbname=testdb user=postgres password=dlwnguq1! host=localhost port=5432" };
        if (conn.is_open())
        {
            std::cout << "Connected to " << conn.dbname() << std::endl;
        }

        pqxx::work txn{ conn };
        pqxx::result r = txn.exec("SELECT version()");

        for (auto row : r)
        {
            std::cout << "PostgreSQL version: " << row[0].c_str() << std::endl;
        }

        txn.commit();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}