#include <pqxx/pqxx>
#include <iostream>

int main()
{
    try
    {
        pqxx::connection conn("host=127.0.0.1 port=5432 dbname=test user=postgres password=1234");

        if (!conn.is_open())
        {
            std::cout << "DB connection failed\n";
            return 0;
        }

        pqxx::work txn(conn);

        pqxx::result r = txn.exec("SELECT id, name FROM users");

        for (auto row : r)
        {
            int id = row["id"].as<int>();
            std::string name = row["name"].as<std::string>();

            std::cout << id << " / " << name << "\n";
        }

        txn.commit();
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
    }

    return 0;
}