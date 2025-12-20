package org.nunotaba.CreationalPattern.Factory;

// 就是根据不同的参数,返回不同的实现细节的类
interface DatabaseConnection {
    void getConnect();
}

class MySQLConnection implements DatabaseConnection {
    @Override
    public void getConnect() {
        System.out.println("MySQL connects successfully!");
    }
}

class OracleConnection implements DatabaseConnection {
    @Override
    public void getConnect() {
        System.out.println("Oracle connects successfully!");
    }
}

class ConnectionFactory {
    public ConnectionFactory () {

    }

    public DatabaseConnection getConnection(String databaseName) {
        switch (databaseName) {
            // 利用不同接口的实现来封装细节
            case "MySQL": return new MySQLConnection();
            case "Oracle": return new OracleConnection();
            default: throw new ClassCastException("Sorry, we don't support this kind of database YET!");
        }
    }
}



public class FactoryDemo {
    public static void main(String[] args) {
        ConnectionFactory connectionFactory = new ConnectionFactory();
        DatabaseConnection databaseConnection = connectionFactory.getConnection("MySQL");
        databaseConnection.getConnect();
    }
}
