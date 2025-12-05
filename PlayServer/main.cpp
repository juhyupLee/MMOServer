#include <pqxx/pqxx>
#include <iostream>


using SQL_Value = std::variant<
    std::reference_wrapper<int8_t>,
    std::reference_wrapper<int16_t>,
    std::reference_wrapper<int32_t>,
    std::reference_wrapper<int64_t>,
    std::reference_wrapper<float>,
    std::reference_wrapper<double>,
    std::reference_wrapper<std::string>
>;

using RowData = std::vector<SQL_Value>;

class Player
{
public:
    int64_t m_accountUID;
	std::string m_sessionKey;
    int64_t m_updateTime;

};

auto PlayerBinder = [](std::shared_ptr<Player> player)
    {
    return RowData
    {
        player->m_accountUID,
        player->m_sessionKey,
        player->m_updateTime
    };
};


template <typename T, typename Binder> 
bool Select(pqxx::result& result, std::vector<std::shared_ptr<T>>& outList, Binder& binder)
{ 
    for (auto row : result)
    {
        int index = 0;
        auto object = std::make_shared<T>();
        auto rowDatas = binder(object);
        for (auto field : row)
        {
            switch (field.type())
            {
                case 21: // int16_t
                {
                    std::get<std::reference_wrapper<int16_t>>(rowDatas[index++]).get() = field.as<int16_t>();
                    break;
                }
                case 23: // int32_t
                {
                    std::get<std::reference_wrapper<int32_t>>(rowDatas[index++]).get() = field.as<int32_t>();
                    break;
                }
                case 20: // int64_t
                {
                    std::get<std::reference_wrapper<int64_t>>(rowDatas[index++]).get() = field.as<int64_t>();
                    break;
                }
                case 700: // float
                {
                    std::get<std::reference_wrapper<float>>(rowDatas[index++]).get() = field.as<float>();
                    break;
                }
                case 701: // double
                {
                    std::get<std::reference_wrapper<double>>(rowDatas[index++]).get() = field.as<double>();
                    break;
                }
                case 1114: // timeStamp with time zone
                case 1184: // timeStamp with time zone
                {
                    auto temp = field.as<std::string>();
                    std::get<std::reference_wrapper<int64_t>>(rowDatas[index++]).get() = TimeUtil::VariantTimeToEpochTime(temp);
                    break;
                }
                case 25: // text
                {
                    //TimeUtil::ToEpochTime()
                    std::get<std::reference_wrapper<std::string>>(rowDatas[index++]).get() = field.as<std::string>();
                    break;
                }
            }
        }
        outList.push_back(object);

    }
    return true;
}
int main()
{
    try
    {
        pqxx::connection conn("host=127.0.0.1 port=5432 dbname=postgres user=postgres password=dlwnguq1!");
        if (!conn.is_open())
        {
            return 0;

            std::cout << "DB connection failed\n";
        }
        pqxx::work txn(conn);
        txn.exec("INSERT INTO account (account_uid, session_key) VALUES (1,'Player1')");
        txn.exec("INSERT INTO account (account_uid, session_key) VALUES (2,'Player2')");
        pqxx::result r = txn.exec("SELECT account_uid, session_key,update_time FROM account");

        std::vector<std::shared_ptr<Player>> playerList;
        
        Select(r,playerList, PlayerBinder);


        txn.commit();
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
    }

    return 0;
}