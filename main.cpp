#include <QApplication>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QObject>
#include <QDebug>
#include <QMessageBox>
#include <QUrlQuery>
#include <QUrl>
#include <QMap>
#include <QFile>
#include <QTextCodec>

//Инициализация QSql
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

//Умные указатели
#include <QPointer>

//Подключаю библиотеки Windows для создания окна консоли
#include <windows.h>

#include <QProcess>

//Общедоступный объект, осуществляющий подключение к базе данных
QSqlDatabase db;

//Функция для разбора GET параметров у ссылки
QMap<QString,QString> ParseUrlParameters(QString &url)
{
    //Заранее создаем коллекцию для возвращаемого значения
    QMap<QString,QString> ret;

    //Параметров нет. Возвращаем пустую коллекцию
    if(url.indexOf('?')==-1)
    {
        return ret;
    }

    //Обрезаем из url все до вопроса
    QString tmp = url.right(url.length()-url.indexOf('?')-1);

    //Разбиваем в коллекцию параметров через разделитель &
    QStringList paramlist = tmp.split('&');

    //Ввожу параметры в коллекцию
    for(int i=0;i<paramlist.count();i++)
    {
        QStringList paramarg = paramlist.at(i).split('=');
        ret.insert(paramarg.at(0),paramarg.at(1));
    }

    //Вывожу в консоль полученные параметры
    QMapIterator<QString, QString> i(ret);
    while (i.hasNext()) {
        i.next();
        qDebug() << i.key() << ":" << i.value() << endl;
    }

    return ret;
}

//Этот класс собирает код html для ответа клиенту по текущей ссылке
class HtmlResponse{

private:
    //Перечисление страниц. Страница должна начинаться с /. Допустимо слитное написание для подстраниц
    QStringList pages;

    //HTML код главной страницы
    const QString mainpage="<table width='100%' height='100%' ><tr><td align='center'>"
                           "<h2>База данных ГИБДД</h2>"
                           "<a href='/persons' target='_self'>Данные граждан</a><br><a href='/autos' target='_self'>Данные автомобилей</a>"
                           "</td></tr></table>";

    const QString infopage="<h2>{TITLE}</h2><br><p>{INFO}</p><hr><a href='{BACKURL}'>Назад</a>";

public:
    HtmlResponse()
    {
        //Запись страниц
        pages<<"/"<<"/index"<<"/persons"<<"/persons/add"<<"/autos"<<"/autos/add";
    }

    QString GetHtml(QString request)
    {
        QMap<QString,QString> params=ParseUrlParameters(request);

        //Параметры были. Для упрощенной обработки их стоит удалить
        if(params.count()!=0)
        {
            request.remove(request.indexOf('?'),request.length()-request.indexOf('?'));
        }

        qDebug() << "Запрос на страницу "<<request;

        QString response;
        switch(pages.indexOf(request))
        {
            //Главная страница. Выводит фрейм
            case 0:
            {
                response = "<html><head><HTA:APPLICATION APPLICATIONNAME='БазаГИБДД' ID='БазаГИБДД' VERSION='1.0'/></head><body>"
                           "<iframe src=\"http://127.0.0.1:1488/index\" width=\"100%\" height=\"100%\" application=\"yes\"> "
                           "</body></html>";
            }
            break;

            //Фрейм меню выбора страницы
            case 1:
            {
                response = mainpage;
            }
            break;

            //Граждане
            case 2:
            {
                response = "<h2>Владельцы</h2><table style='width: 100%; border: 1px solid black;' border='1'>";
                QSqlQuery query;
                query.exec("SELECT  Код, ФИО,Адрес,Телефон,Комментарий FROM Граждане;");
                while (query.next())
                {
                    response+="<tr><th>"+query.value(0).toString()+"</th><td align='center'>"+query.value(1).toString()+"</td><td align='center'>"+query.value(2).toString()+"</td><td align='center'>"+query.value(3).toString()+"</td><td align='center'>"+query.value(4).toString()+"</td></tr>";
                }
                response+="</table><h4>Добавить владельца</h4>"
                          "<form action='/persons/add' method='get'>"
                          "<span>ФИО: <input type='text' name='fio'></span><br>"
                          "<span>Адрес: <input type='text' name='adres'></span><br>"
                          "<span>Телефон: <input type='text' name='phone'></span><br>"
                          "<span>Комментари: <input type='text' name='comment'></span><br>"
                          "<input type='submit' value='Отправить'>"
                          "</form><hr><a href='/index'>Назад</a>";
            }
            break;

            //Добавление гражданина
            case 3:
            {
                //Преобразование из "процентного вида <form>" в человеческий вид
                QString fio=QUrl::fromPercentEncoding(QByteArray::fromStdString(params.value("fio").toStdString()));
                QString adres=QUrl::fromPercentEncoding(QByteArray::fromStdString(params.value("adres").toStdString()));
                QString phone=QUrl::fromPercentEncoding(QByteArray::fromStdString(params.value("phone").toStdString()));
                QString comment=QUrl::fromPercentEncoding(QByteArray::fromStdString(params.value("comment").toStdString()));
                fio.replace("+"," ");
                adres.replace("+"," ");
                phone.replace("+"," ");
                comment.replace("+"," ");

                qDebug() << "Добавление в базу данных владельца "+fio;
                response = infopage;
                if(fio.length()!=0&&adres.length()!=0&&phone.length()!=0&&comment.length()!=0)
                {
                    QSqlQuery query;
                    if(query.exec("INSERT INTO Граждане ( ФИО,Адрес,Телефон,Комментарий ) VALUES ('"+fio+"', '"+adres+"', '"+phone+"', '"+comment+"')"))
                    {
                        response =  response.replace("{TITLE}","Запись успешно добавлена!").replace("{INFO}","Добавлен владелец с именем "+fio).replace("{BACKURL}","/persons");
                    }
                    else
                    {
                        response =  response.replace("{TITLE}","Произошла ошибка!").replace("{INFO}",query.lastError().text()).replace("{BACKURL}","/persons");
                    }
                }
                else
                {
                    response =  response.replace("{TITLE}","Произошла ошибка!").replace("{INFO}","Не заданы параметры!").replace("{BACKURL}","/persons");
                }
            }
            break;

            //Автомобили
            case 4:
            {
                response = "<h2>Автомобили</h2><table style='width: 100%; border: 1px solid black;' border='1'>";
                QSqlQuery query;
                query.exec("SELECT  Код, НомерКузова,НомерДвигателя,Модель,Владелец FROM Автомобили;");
                while (query.next())
                {
                    response+="<tr><th>"+query.value(0).toString()+"</th><td align='center'>"+query.value(1).toString()+"</td><td align='center'>"+query.value(2).toString()+"</td><td align='center'>"+query.value(3).toString()+"</td><td align='center'>"+query.value(4).toString()+"</td></tr>";
                }
                response+="</table><h4>Добавить авто</h4>"
                          "<form action='/autos/add' method='get'>"
                          "<span>Номер кузова: <input type='text' name='kuzov'></span><br>"
                          "<span>Номер двигателя: <input type='text' name='dvigatel'></span><br>"
                          "<span>Модель: <input type='text' name='model'></span><br>"
                          "<span>Номер владельца: <input type='text' name='vladelec'></span><br>"
                          "<input type='submit' value='Отправить'>"
                          "</form><hr><a href='/index'>Назад</a>";
            }
            break;

            //Добавление автомобиля
            case 5:
            {
                //Преобразование из "процентного вида <form>" в человеческий вид
                QString kuzov=QUrl::fromPercentEncoding(QByteArray::fromStdString(params.value("kuzov").toStdString()));
                QString dvigatel=QUrl::fromPercentEncoding(QByteArray::fromStdString(params.value("dvigatel").toStdString()));
                QString model=QUrl::fromPercentEncoding(QByteArray::fromStdString(params.value("model").toStdString()));
                QString vladelec=QUrl::fromPercentEncoding(QByteArray::fromStdString(params.value("vladelec").toStdString()));
                kuzov.replace("+"," ");
                dvigatel.replace("+"," ");
                model.replace("+"," ");
                vladelec.replace("+"," ");

                qDebug() << "Добавление в базу данных автомобиля "+model;
                response = infopage;
                if(kuzov.length()!=0&&dvigatel.length()!=0&&model.length()!=0&&vladelec.length()!=0)
                {
                    QSqlQuery query;
                    if(query.exec("INSERT INTO Автомобили ( НомерКузова,НомерДвигателя,Модель,Владелец ) VALUES ('"+kuzov+"', '"+dvigatel+"', '"+model+"', '"+vladelec+"')"))
                    {
                        response =  response.replace("{TITLE}","Запись успешно добавлена!").replace("{INFO}","Добавлен автомобиль модели "+model).replace("{BACKURL}","/autos");
                    }
                    else
                    {
                        response =  response.replace("{TITLE}","Произошла ошибка!").replace("{INFO}",query.lastError().text()).replace("{BACKURL}","/autos");
                    }
                }
                else
                {
                    response =  response.replace("{TITLE}","Произошла ошибка!").replace("{INFO}","Не заданы параметры!").replace("{BACKURL}","/autos");
                }
            }

            default:
                response = "<h2>404</h2>";
            break;
        }

        return response;
    }
}ResponseManager;

//Этот класс осуществляет низкоуровневую обработку запросов к серверу
class ConnectionManager
{
private:
    QTcpServer *tcpServer;
    QMap<int,QTcpSocket *> SClients;

public:
    ConnectionManager()
    {
        tcpServer = new QTcpServer();

        //Ожидание запроса на подключение
        QObject::connect(tcpServer,QTcpServer::newConnection,[this]()
        {
            qDebug()<<"Новое подключение!";

            //Выделение сокета и передача управления обработчику
            QTcpSocket* clientSocket=tcpServer->nextPendingConnection();
            int idusersocs=clientSocket->socketDescriptor();
            SClients[idusersocs]=clientSocket;
            QObject::connect(clientSocket,QTcpSocket::readyRead,[clientSocket,this](){
                //Для удобства получаем указатель на сокет
                QTcpSocket* tempClientSocket = clientSocket;

                //Считывание запроса
                QString request;


                if(tempClientSocket->bytesAvailable())
                {
                    qDebug() << "Чтение сокета";
                    QString tmp= tempClientSocket->readAll();
                    request=tmp.split(" ").at(1);
                    qDebug() << "Считано: "<<request;
                }
                else
                {
                    qDebug() << "Сокет отброшен: данные не получены!";
                    tempClientSocket->close();
                }



                //Вывод ответа клиенту
                QTextStream os(tempClientSocket);
                os.setAutoDetectUnicode(true);
                os << "HTTP/1.0 200 Ok\r\n";
                os << "Content-Type: text/html; charset=\"utf-8\"\r\n";
                os << "\r\n";
                os << ResponseManager.GetHtml(request);
                os << "\n";
                tempClientSocket->close();

                SClients.remove(tempClientSocket->socketDescriptor());
            });
        });

        //Запуск сервера
        if (!tcpServer->listen(QHostAddress::LocalHost, 1488)) {
            qDebug() << "Ошибка: порт занят!";
            QMessageBox msg;
            msg.setText("Сервер не запущен, порт занят!");
            msg.exec();
        } else {
            qDebug() << "Сервер запущен!";

            //Запустить интерпретатор веб приложений Microsoft на весь экран
            Sleep(1000);
            system("cmd /c start /max mshta http://127.0.0.1:1488/");
        }
    }
};

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //Устанавливаю кодировку QString в UTF8
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    //Подключаюсь к базе данных
    db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName("Driver={Microsoft Access Driver (*.mdb, *.accdb)};DSN='';DBQ=C:\\Projects\\CarDb\\CarDb.mdb");
    if(!db.open())
    {
        QMessageBox msg;
        msg.setText("Не удается подключиться к базе данных!"+db.lastError().text());
        msg.exec();
        return 0;
    };

    ConnectionManager manager;

    //Закрытие базы данных при завершении
    int exec = a.exec();
    db.close();
    return exec;
}
