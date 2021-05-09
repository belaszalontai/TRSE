   /*
 * Turbo Rascal Syntax error, “;” expected but “BEGIN” (TRSE, Turbo Rascal SE)
 * 8 bit software development IDE for the Commodore 64
 * Copyright (C) 2018  Nicolaas Ervik Groeneboom (nicolaas.groeneboom@gmail.com)
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program (LICENSE.txt).
 *   If not, see <https://www.gnu.org/licenses/>.
*/

#include "parser.h"
#include "source/Compiler/data_pmm.h"

QStringList Parser::s_usedTRUs;
QStringList Parser::s_usedTRUNames;
QVector<QSharedPointer<Parser>> Parser::m_tpus;



QString Parser::WashVariableName(QString v)
{
  //  QString p =m_symTab->m_gPrefix;
//    p = p.replace("_","::");
    v = v.replace(m_symTab->m_gPrefix,"");
    return v;

}

QString Parser::VerifyVariableName(QString v)
{
    if (m_isTRU)
        return v;
    if (Syntax::s.m_currentSystem->m_renameVariables.contains(v.toLower())) {
        return Syntax::s.m_currentSystem->m_renamedVariablePrefix+v;
    }
    return v;
}


QSharedPointer<Symbol> Parser::getSymbol(QSharedPointer<Node> var)
{
    auto v = qSharedPointerDynamicCast<NodeVar>(var);
    if (v==nullptr)
        return nullptr;
    QString val = v->value;
    val = val.replace("#","");
   return m_symTab->Lookup(val,v->m_op.m_lineNumber);

}

QStringList Parser::getFlags() {
    QStringList flags;


    bool done = false;

    m_typeFlags[TokenType::CHIPMEM] = "chipmem";
    m_typeFlags[TokenType::PPURE] = "pure";
    m_typeFlags[TokenType::PURE_VARIABLE] = "pure_variable";
    m_typeFlags[TokenType::PURE_NUMBER] = "pure_number";
    m_typeFlags[TokenType::COMPRESSED] = "compressed";
    m_typeFlags[TokenType::WRAM] = "wram";
    m_typeFlags[TokenType::SPRRAM] = "sprram";
    m_typeFlags[TokenType::RAM] = "ram";
    m_typeFlags[TokenType::HRAM] = "hram";
    m_typeFlags[TokenType::ALIGNED] = "aligned";
    m_typeFlags[TokenType::NO_TERM] = "no_term";
    m_typeFlags[TokenType::INVERT] = "invert";
    m_typeFlags[TokenType::SIGNED] = "signed";
    m_typeFlags[TokenType::GLOBAL] = "global";

    while (!done)  {
        done = true;
        TokenType::Type type = m_currentToken.m_type;
        if (m_typeFlags.contains(type)) {
            Eat(type);
            flags<<m_typeFlags[type];
            done = false;
        }
        // Bank is a special case :/
        if (m_currentToken.m_type==TokenType::BANK) {
            Eat(TokenType::BANK);
            done = false;
            Eat(TokenType::LPAREN);
            flags<<"bank"<<Util::numToHex(GetParsedInt(TokenType::INTEGER_CONST));
            Eat(TokenType::RPAREN);

        }
    }
    return flags;

}

Parser::Parser()
{
}

void Parser::Delete()
{
    for (QString val : m_procedures.keys()) {
        QSharedPointer<Node> s = m_procedures[val];
        // if (s!=nullptr) {
        //s->Delete();
        //            delete s;
        //        }
    }
    m_procedures.clear();
    //delete m_symTab;
    //m_symTab = nullptr;
}

void Parser::InitObsolete()
{
    m_obsoleteWarnings.clear();
    m_obsoleteWarnings.append(QStringList() << "rand"<<"Funtion 'Rand()' is scheduled to be deprecated from 0.09. Please use 'Random()' instead. ");
    m_obsoleteWarnings.append(QStringList() << "inczp"<<"Funtion 'IncZP()' is scheduled to be deprecated from 0.09. Please use 'zp:=zp+value;' to increase pointers instead. ");
    m_obsoleteWarnings.append(QStringList() << "writeln"<<"Funtion 'Writeln()' is scheduled to be deprecated from 0.09. Please use 'PrintString()' instead. ");
//    m_obsoleteWarnings.append(QStringList() << "copycharsetfromrom"<<"Funtion 'CopyCharsetFromROM()' is scheduled to be deprecated from 0.09. ");
}


void Parser::Eat(TokenType::Type t)
{
//    if (m_tick++%500==99)
//        emit EmitTick(".");
    if (m_prevPercent!=m_lexer->getPositionInPercent()) {
        m_prevPercent = m_lexer->getPositionInPercent();

     emit EmitTick("&"+QString::number(m_prevPercent));
    }
//   qDebug() << m_currentToken.m_value << m_currentToken.m_intVal;
    if (m_currentToken.m_type == t) {
        m_currentToken = m_lexer->GetNextToken();
//        if (m_pass==2)
  //          qDebug() << "Token : " <<m_currentToken.getType() <<m_currentToken.m_value << m_currentToken.m_intVal;
        int cnt =0;
        while (m_currentToken.m_type==TokenType::PREPROCESSOR && m_pass>PASS_PREPRE && m_pass!=PASS_OTHER  && cnt++<32 ) {
            HandlePreprocessorInParsing();
          //  qDebug() << "Inside handle: " << m_currentToken.m_value;

//            qDebug() <<cnt++ <<m_currentToken.m_value;
        }

    }
    else {
        QString warning = "\nDid you forget a semicolon (;) ?";
        ErrorHandler::e.Error("Expected '" + TokenType::getType(t) + "' but found '" +m_currentToken.m_value+"'" + warning,m_currentToken.m_lineNumber);
    }
/*    if (m_currentToken.m_type==TokenType::TEOF) {
        qDebug() << m_currentToken.getType();
        ErrorHandler::e.Error("Syntax errror", m_currentToken.m_lineNumber);
    }
*/
}

void Parser::Eat()
{
    Eat(m_currentToken.m_type);
}


int Parser::findSymbolLineNumber(QString symbol)
{
    int i=1;
    for (QString& s: m_lexer->m_lines) {
        i++;
        if (s.contains(symbol))
            return i;
    }
    return 1;
}

void Parser::InitBuiltinFunctions()
{
    m_initJumps.clear();
    int oldBlock = Node::m_staticBlockInfo.m_blockID;
    if (m_preprocessorDefines.contains("BuiltinMethodsLocation")) {

        ParserBlock pb;
        pb.m_blockID = m_parserBlocks.count();
        pb.pos = m_preprocessorDefines["BuiltinMethodsLocation"];
        pb.pos = Util::numToHex(pb.pos.toInt());
        m_parserBlocks.append(pb);
        Node::m_staticBlockInfo.m_blockID = pb.m_blockID;
        Node::m_staticBlockInfo.m_blockPos = pb.pos;
        Node::m_staticBlockInfo.m_blockName = "BuiltinMethods";
    }

/*    if (Syntax::s.m_currentSystem->m_system == Syntax::NES)
        InitBuiltinFunction(QStringList()<< "*", "init8x8mulNes");
    else*/
    //m; InitP61Player; Amiga;
    //m; PlayP61Mod; Amiga; a

   if (Syntax::s.m_currentSystem->m_system == AbstractSystem::AMIGA) {

       InitBuiltinFunction(QStringList()<< "playp61module"<<"initp61module" , "initp61playerinternal");
    }

   if (Syntax::s.m_currentSystem->m_system!=AbstractSystem::X86) {
       InitBuiltinFunction(QStringList()<< "SetPixelCGA", "init_cga_scanlines");
   }


    if (Syntax::s.m_currentSystem->m_system == AbstractSystem::C64 ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::C128 ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::PLUS4 ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::NES ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::PET ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::OK64 ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::ATARI800 ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::X16 ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::BBCM ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::MEGA65 ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::VIC20 ||
            Syntax::s.m_currentSystem->m_system == AbstractSystem::BBCM  ) {
/*        InitBuiltinFunction(QStringList()<< "*", "initeightbitmul");

        InitBuiltinFunction(QStringList()<< "*", "init16x8mul");
        InitBuiltinFunction(QStringList()<< "/", "init8x8div");
        InitBuiltinFunction(QStringList()<< "/", "init16x8div");
        */
//          qDebug() << Node::flags.keys();
        if (Node::flags.contains("mul8"))
            InitBuiltinFunction(QStringList()<< "", "initeightbitmul");
        if (Node::flags.contains("mul16"))
            InitBuiltinFunction(QStringList()<< "", "init16x8mul");
        if (Node::flags.contains("div16"))
            InitBuiltinFunction(QStringList()<< "", "init16x8div");
        if (Node::flags.contains("div8"))
            InitBuiltinFunction(QStringList()<< "", "init8x8div");

        if (Syntax::s.m_currentSystem->m_system == AbstractSystem::C64 ||
                Syntax::s.m_currentSystem->m_system == AbstractSystem::C128) {
            InitBuiltinFunction(QStringList()<< "getkey(", "initgetkey");
        }
        InitBuiltinFunction(QStringList()<< "rand(", "initrandom","init_random_call");
        InitBuiltinFunction(QStringList()<< "random(", "initrandom256");

        InitBuiltinFunction(QStringList()<< "rasterirqwedge(" , "init_wedge");
        InitBuiltinFunction(QStringList()<< "playvic20sid(" , "init_vic20_sidplay");
        InitBuiltinFunction(QStringList()<< "viairq(" , "init_viairq");
        InitBuiltinFunction(QStringList()<< "viarasterirq(" , "init_viairq");
        InitBuiltinFunction(QStringList()<< "initmodplayer(" , "include_modplayer");
        InitBuiltinFunction(QStringList()<< "decrunch("<<"decrunchfromindex(", "init_decrunch");

        if (Syntax::s.m_currentSystem->m_system!=AbstractSystem::NES && Syntax::s.m_currentSystem->m_system!=AbstractSystem::ATARI520ST)
            InitBuiltinFunction(QStringList()<< "sine[", "initsinetable", "initsine_calculate");

        InitBuiltinFunction(QStringList()<< "log2_table[" << "atan2(", "initlog2");

        InitBuiltinFunction(QStringList()<< "atan2(", "initatan2");


        InitBuiltinFunction(QStringList()<< "sqrt(", "initsqrt16");
        InitBuiltinFunction(QStringList()<< "printdecimal(", "initprintdecimal");
        InitBuiltinFunction(QStringList()<< "moveto80(", "initmoveto80");
        InitBuiltinFunction(QStringList()<< "moveto(" << "printstring(" << "tile(", "initmoveto");
        InitBuiltinFunction(QStringList()<< "printstring(" << "printnumber(", "initprintstring");

        InitBuiltinFunction(QStringList()<< "drawtextbox(" << "drawcolortextbox(", "initdrawtextbox");

        InitBuiltinFunction(QStringList()<< "joystick(" , "initjoystick");
        InitBuiltinFunction(QStringList()<< "readjoy1(" , "initjoy1");
        InitBuiltinFunction(QStringList()<< "readjoy2(" , "initjoy2");

        InitBuiltinFunction(QStringList()<< "bcdprint(", "initbcd");

        InitBuiltinFunction(QStringList()<< "vbmtesttilepixel(", "initVbmTestTilePixel");
        InitBuiltinFunction(QStringList()<< "vbmtesttilepixel2(", "initVbmTestTilePixel2");

        InitBuiltinFunction(QStringList()<< "vbmsetdisplaymode(", "initVbm");
        InitBuiltinFunction(QStringList()<< "vbmsetposition1(" << "vbmsetposition2(" << "vbmsetposition4(", "initVbmSetPosition");
        InitBuiltinFunction(QStringList()<< "vbmclear(", "initVbmClear");
        InitBuiltinFunction(QStringList()<< "vbmnextcolumn(", "initVbmNextColumn");

        InitBuiltinFunction(QStringList()<< "vbmdrawdot(", "initVbmDrawDot");
        InitBuiltinFunction(QStringList()<< "vbmcleardot(", "initVbmClearDot");
        InitBuiltinFunction(QStringList()<< "vbmdrawdote(", "initVbmDrawDotE");
        InitBuiltinFunction(QStringList()<< "vbmtestpixel(", "initVbmTestPixel");
        InitBuiltinFunction(QStringList()<< "vbmdrawdot("<< "vbmcleardot("<< "vbmdrawdote("<< "vbmtestpixel(", "initVbmDot");

        InitBuiltinFunction(QStringList()<< "vbmdrawblot(", "initVbmDrawBlot");
        InitBuiltinFunction(QStringList()<< "vbmclearblot(", "initVbmClearBlot");
        InitBuiltinFunction(QStringList()<< "vbmdrawblote(", "initVbmDrawBlotE");
        InitBuiltinFunction(QStringList()<< "vbmtestpixel2(", "initVbmTestPixel2");
        InitBuiltinFunction(QStringList()<< "vbmdrawblot("<< "vbmclearblot("<< "vbmdrawblote("<< "vbmtestpixel2(", "initVbmBlot");

        InitBuiltinFunction(QStringList()<< "vbmscrollleft(", "initVbmScrollLeft");
        InitBuiltinFunction(QStringList()<< "vbmscrollright(", "initVbmScrollRight");
        InitBuiltinFunction(QStringList()<< "vbmscrollfixtop(", "initVbmScrollFixTop");
        InitBuiltinFunction(QStringList()<< "vbmscrollfixbottom(", "initVbmScrollFixBottom");

        InitBuiltinFunction(QStringList()<< "vbmdrawtilemap(", "initVbmDrawTileMap");
        InitBuiltinFunction(QStringList()<< "vbmdrawtilemapo(", "initVbmDrawTileMapO");
        InitBuiltinFunction(QStringList()<< "vbmdrawtilemape(", "initVbmDrawTileMapE");
        InitBuiltinFunction(QStringList()<< "vbmcleartilemap(", "initVbmClearTileMap");

        // must now call manually to set up for screen mode
        //InitBuiltinFunction(QStringList()<< "vbmscreenshiftleft(", "initVbmScreenShiftLeft");
        //InitBuiltinFunction(QStringList()<< "vbmscreenshiftright(", "initVbmScreenShiftRight");

        InitBuiltinFunction(QStringList()<< "vbmspritestitch(", "initVbmSpriteStitch");
        InitBuiltinFunction(QStringList()<< "vbmspriteshiftr(", "initVbmSpriteShiftR");
        InitBuiltinFunction(QStringList()<< "vbmspriteshiftl(", "initVbmSpriteShiftL");
        InitBuiltinFunction(QStringList()<< "vbmspriteshiftsr(", "initVbmSpriteShiftSR");
        InitBuiltinFunction(QStringList()<< "vbmspriteshiftsl(", "initVbmSpriteShiftSL");

        InitBuiltinFunction(QStringList()<< "vbmdrawsprite8(", "initVbmDrawSprite8");
        InitBuiltinFunction(QStringList()<< "vbmdrawsprite8e(", "initVbmDrawSprite8E");
        InitBuiltinFunction(QStringList()<< "vbmclearsprite8(", "initVbmClearSprite8");

        InitBuiltinFunction(QStringList()<< "vbmdrawsprite(", "initVbmDrawSprite");
        InitBuiltinFunction(QStringList()<< "vbmdrawspritee(", "initVbmDrawSpriteE");
        InitBuiltinFunction(QStringList()<< "vbmclearsprite(", "initVbmClearSprite");

        InitBuiltinFunction(QStringList()<< "vbmdrawsprite2(", "initVbmDrawSprite2");
        InitBuiltinFunction(QStringList()<< "vbmdrawsprite2e(", "initVbmDrawSprite2E");
        InitBuiltinFunction(QStringList()<< "vbmclearsprite2(", "initVbmClearSprite2");

        InitBuiltinFunction(QStringList()<< "vbmdrawsprite16(", "initVbmDrawSprite16");
        InitBuiltinFunction(QStringList()<< "vbmdrawsprite16e(", "initVbmDrawSprite16E");
        InitBuiltinFunction(QStringList()<< "vbmclearsprite16(", "initVbmClearSprite16");

        InitBuiltinFunction(QStringList()<< "vbmdrawspriteslice(", "initVbmDrawSpriteSlice");
        InitBuiltinFunction(QStringList()<< "vbmdrawspriteslicee(", "initVbmDrawSpriteSliceE");
        InitBuiltinFunction(QStringList()<< "vbmclearspriteslice(", "initVbmClearSpriteSlice");

        InitBuiltinFunction(QStringList()<< "vbmdrawtext(", "initVbmDrawText");
        InitBuiltinFunction(QStringList()<< "vbmdrawtexto(", "initVbmDrawTextO");
        InitBuiltinFunction(QStringList()<< "vbmdrawtexte(", "initVbmDrawTextE");
        InitBuiltinFunction(QStringList()<< "vbmcleartext(", "initVbmClearText");

        InitBuiltinFunction(QStringList()<< "vbmdrawsmalltexto(", "initVbmDrawSmallTextO");
        InitBuiltinFunction(QStringList()<< "vbmdrawsmalltexte(", "initVbmDrawSmallTextE");
        InitBuiltinFunction(QStringList()<< "vbmclearsmalltext(", "initVbmClearSmallText");

        InitBuiltinFunction(QStringList()<< "vbmdrawbcd(", "initVbmDrawBCD");
        InitBuiltinFunction(QStringList()<< "vbmdrawsmallbcd(", "initVbmDrawSmallBCD");
        InitBuiltinFunction(QStringList()<< "vbmdrawsmallbcdo(", "initVbmDrawSmallBCDO");

        InitBuiltinFunction(QStringList()<< "vbmcopytobuffer(", "initVbmCopyToBuffer");
        InitBuiltinFunction(QStringList()<< "vbmcopyfrombuffer(", "initVbmCopyFromBuffer");


    }
    if (m_preprocessorDefines.contains("BuiltinMethodsLocation")) {
        Node::m_staticBlockInfo.m_blockID = oldBlock;
        Node::m_staticBlockInfo.m_blockPos = "";
        Node::m_staticBlockInfo.m_blockName = "";
    }
}

void Parser::VerifyInlineSymbols6502(QString s)
{
    QStringList lst = s.split("\n");
    for (QString l : lst) {
        QStringList l2 = l.trimmed().simplified().split(" ");
        if (l2.count()>1) {
            QString s2 = l2[1];
            s2 = s2.remove("(").remove(")").remove(",x").remove(",y");
            if (m_symTab->m_symbols.contains(s2))
                m_symTab->Lookup(s2,m_currentToken.m_lineNumber);
            if (m_procedures.contains(s2))
                m_procedures[s2]->m_isUsed = true;
        }
    }
}

void Parser::InitBuiltinFunction(QStringList methodName, QString builtinFunctionName, QString initJump )
{
    if (m_ignoreMethods.contains(builtinFunctionName.toLower())) {
            return;
    }
    QString txt = m_lexer->m_text.toLower();
    // Remove comments  /* */ and // from text
    QRegularExpression expBlockComment("/\\*([^*]|[\\r\\n]|(\\*+([^*/]|[\\r\\n])))*\\*+/");
    QRegularExpression expEndComment("(\\/\\/[^\\n\\r]*(?:[\\n\\r]+|$))");
    txt = txt.replace(expBlockComment,"");
    txt = txt.replace(expEndComment,"");
//    qDebug().noquote() << txt;
    for (QString s: methodName)
     if (txt.contains(s)) {
         m_procedures[builtinFunctionName] = QSharedPointer<NodeProcedureDecl>(new NodeProcedureDecl(Token(TokenType::PROCEDURE, builtinFunctionName), builtinFunctionName));
         m_ignoreBuiltinFunctionTPU.append(builtinFunctionName);
        if (initJump!="")
            m_initJumps << "\tjsr "+ initJump;
        return;
     }

}

void Parser::VerifyToken(Token t)
{
    //if (Syntax::s.globals.contains(t.m_value))
    //    return;

    // ErrorHandler::e.Error("Does not recognize '"+t.m_value + "'");
}

void Parser::InitSystemPreprocessors()
{
    m_preprocessorDefines[AbstractSystem::StringFromSystem(Syntax::s.m_currentSystem->m_system)] = "1";
    Syntax::s.m_currentSystem->InitSystemPreprocessors(m_preprocessorDefines);

}

void Parser::InitSystemSymbols()
{
    if (Syntax::s.m_currentSystem->m_processor == AbstractSystem::MOS6502)
        if (m_projectIni->contains("zeropages_userdefined")) {
            int cur = 1;
            for (QString s : m_projectIni->getStringList("zeropages_userdefined")) {
                QString name = "TEMP_VAR"+QString::number(cur++);
                        m_symTab->m_constants[name] = QSharedPointer<Symbol>(
                                    new Symbol(name,"ADDRESS",Util::NumberFromStringHex(s)));
            }
        }

}

void Parser::PreprocessIfDefs(bool ifdef)
{

    //Eat();
     QString key="";
     bool isIf = false;
     QString keyValue ="";
     //if (m_currentToken.m_valu)
     if (m_currentToken.m_value=="else") {
//         qDebug() << "WE ARE ELSE";
         Eat();
         if (m_lastKey.count()!=0)
             key = m_lastKey.last();
     }
      else
     {
         // ifdef, ifndef, if
         if (m_currentToken.m_value=="if") isIf = true;
         Eat();
         key = m_currentToken.m_value;
         if (isIf) {
             Eat();
             Eat(TokenType::EQUALS);
             keyValue = m_currentToken.m_value;
             if (keyValue=="") keyValue = QString::number(m_currentToken.m_intVal);
//             Eat();
         }
     }
  //   qDebug() << "CURRENTKEY: " <<key;

//    Eat();
     m_lastKey.append(key);
     m_lastIfdef.append(ifdef);

     bool valOK = true;

     // test for if value
     if (isIf && m_preprocessorDefines.contains(key)) {
         QString val = m_preprocessorDefines[key];
  //       qDebug() << "VALS "<<val <<keyValue;
         valOK = (val == keyValue);
     }

    if (ifdef && m_preprocessorDefines.contains(key) && valOK) {
        Eat();
        return; // K
    }

    if (!ifdef && !m_preprocessorDefines.contains(key)) {
        Eat();
        return;
    }
    // Remove everything!
    //qDebug() << "IGNORE" << key;
    m_ignoreAll = true;
    int ignorePop=0; // Counter to keep track of proper ifdef / else /end
    while (!m_lexer->m_finished) {
//        qDebug() << "PARSER " <<m_currentToken.m_value;
        if (m_currentToken.m_type==TokenType::PREPROCESSOR) {

            int org = m_pass;
            m_pass=1;
            // Ignore preprocessors ifdef etc within preprocessors
            if (m_currentToken.m_value.startsWith("if")) {
                ignorePop++;
        //        qDebug() << "INCREASING POP "<<ignorePop;
            }
            Eat();
            m_pass = org;
        }
        else {
            int org = m_pass;
            m_pass = 0;
            Eat(); // OM NOM NOM
            m_pass = org;
        }

        if (m_currentToken.m_type==TokenType::PREPROCESSOR) {
            if (m_currentToken.m_value=="else" && ignorePop==0) {
                Eat();
                m_ignoreAll = false;
                return; // Finish
            }
            if (m_currentToken.m_value=="endif") {
                if (ignorePop==0) {
                Eat();
                m_ignoreAll = false;
                if (m_lastKey.count()==0)
                    ErrorHandler::e.Error("Preprocessor '@endif' mismatch error", m_currentToken.m_lineNumber);
                m_lastKey.removeLast();
                m_lastIfdef.removeLast();
                return; // Finish
                }
                else --ignorePop;
            }
        }
    }


}

void Parser::PreprocessConstants()
{
    QString txt;
    for (QString s: m_lexer->m_text.split("\n")) {
        QVector<QString> numbers;

        QString cur = "";
        for (int i=0;i<s.length();i++) {
            if (s[i]==' ')
                continue;
            if (Syntax::s.binop.contains(QString(s[i].toLower()))) {
                cur+=s[i];
            }
            else if (cur!="") {
                //int i = Util::NumberFromStringHex(cur);
                if (cur.contains('*') || cur.contains('+') || cur.contains('-') || cur.contains('/'))
                numbers.append(cur);
                cur ="";

            }
        }
//        if (numbers.count()!=0)
  //          qDebug() << numbers;

        txt += s +"\n";
    }

    m_lexer->m_text = txt;
}

void Parser::ApplyTPUBefore()
{
    for (QSharedPointer<Parser> p: m_tpus) {
        for (QString k : p->m_procedures.keys()) {
            m_procedures[k] = p->m_procedures[k];
        }

    }
}

void Parser::ApplyTPUAfter(QVector<QSharedPointer<Node>>& declBlock, QVector<QSharedPointer<Node>>& procs)
{
    for (QSharedPointer<Parser> p: m_tpus) {
        if (p->m_hasBeenApplied) {
            continue;
        }

        p->m_hasBeenApplied = true;

        QVector<QSharedPointer<Node>> orgProcs = procs;
        procs.clear();

        QSharedPointer<NodeProgram> np = qSharedPointerDynamicCast<NodeProgram>(p->m_tree);
        QVector<QSharedPointer<Node>> decls;

        for (QSharedPointer<Node> on : np->m_NodeBlock->m_decl) {
            QSharedPointer<NodeProcedureDecl> procDecl =
            qSharedPointerDynamicCast<NodeProcedureDecl>(on);
            if (procDecl!=nullptr) {
                if (!m_ignoreBuiltinFunctionTPU.contains(procDecl->m_procName)) {
                    procs.append(procDecl);
//                    qDebug() << "Appending : " << procDecl->m_procName << procDecl->m_isUsed;
                    m_mergedProcedures.append(procDecl);
                }

            }
            else decls.append(on);


        }

        // Important: add newest procedures *last*
        procs.append(orgProcs);

        QVector<QSharedPointer<Node>> copy;
        for (auto p: declBlock)
                copy.append(p);

        declBlock.clear();
        // Make sure that TRU variables are declared FIRST. Ordering is important for the GB
        declBlock.append(decls);
        declBlock.append(copy);
    }
}

int Parser::getParsedNumberOrConstant() {

    if (m_currentToken.m_value=="") {
        return m_currentToken.m_intVal;
    }
    QSharedPointer<Symbol> s = m_symTab->LookupConstants(m_currentToken.m_value.toUpper());
    if (s==nullptr)
        ErrorHandler::e.Error("Value required to be a number or a constant.",m_currentToken.m_lineNumber);

    return s->m_value->m_fVal;
}

int Parser::GetParsedInt(TokenType::Type forceType) {


    bool done = false;
    QString str = "";
    int p = 0;
//    if (m_pass==0) return 0;
    bool prevNumber = false;

    while (!done) {
//        qDebug() << m_currentToken.getType();

        if (m_currentToken.m_type==TokenType::LPAREN) {
            str = str+ "(";
            p++;
            Eat();
            continue;
        }
        if (m_currentToken.m_type==TokenType::RPAREN) {
            if (p==0) {
//                Eat();
                // OOps! Hit a ")" that is not part of an equation!
                done=true;
                continue;

            }
            str = str+ ")";
            p--;
            Eat();
            continue;
        }

        if (m_currentToken.m_type==TokenType::PLUS) {
            str = str+ "+";
            Eat();
            prevNumber = false;
            continue;
        }
        if (m_currentToken.m_type==TokenType::MINUS) {
            str = str+ "-";
            Eat();
            prevNumber = false;
            continue;
        }
        if (m_currentToken.m_type==TokenType::BITOR) {
            str = str+ "|";
            Eat();
            prevNumber = false;
            continue;
        }
        if (m_currentToken.m_type==TokenType::BITAND) {
            str = str+ "&";
            Eat();
            prevNumber = false;
            continue;
        }
        if (m_currentToken.m_type==TokenType::XOR) {
            str = str+ "^";
            Eat();
            prevNumber = false;
            continue;
        }
        if (m_currentToken.m_type==TokenType::MUL) {
            str = str+ "*";
            Eat();
            prevNumber = false;
            continue;
        }
        if (m_currentToken.m_type==TokenType::DIV) {
            str = str+ "/";
            Eat();
            prevNumber = false;
            continue;
        }
//        qDebug() << "Token type: " <<m_currentToken.getType();
        if (m_currentToken.m_type==TokenType::COMMA || m_currentToken.m_type==TokenType::SEMI || m_currentToken.m_type==TokenType::RBRACKET) {
            if (p!=0)
                ErrorHandler::e.Error("Mismatced paranthesis when parsing number",m_currentToken.m_lineNumber);
            else
            {
                done=true;
                continue;
            }
        }
        if (prevNumber==true) {
//            if (m_currentToken.m_value!="")
  //              str+=QString::number(m_currentToken.m_intVal);

            ErrorHandler::e.Error("Error parsing number : "+str,m_currentToken.m_lineNumber);
        }

        if (m_currentToken.m_value=="") {
//            qDebug() << "ival:"  << QString::number(m_currentToken.m_intVal);
            str+=QString::number(m_currentToken.m_intVal);
            Eat();
            prevNumber = true;
        }
        else {
//            qDebug() << "Looking for constant " << m_currentToken.m_value.toUpper();
  //          qDebug() << m_symTab->m_constants.keys();
            QSharedPointer<Symbol> s = m_symTab->LookupConstants(m_currentToken.m_value.toUpper());

              if (s==nullptr) {
                  done=true;
                  ErrorHandler::e.Error("Could not find constant : '"+m_currentToken.m_value+"'",m_currentToken.m_lineNumber);

              }
              else {
      //            qDebug() << "FOUND adding " <<s->m_value->m_fVal;
               str+=QString::number(s->m_value->m_fVal);
               prevNumber = true;

               Eat();
              }

     //             ErrorHandler::e.Error("Value required to be a number or a constant.",m_currentToken.m_lineNumber);

        }
    }
//    if (p!=0)
    for (int i=0;i<p;i++)
        str+=")";
    str = str.remove("()");

    QJSValue ret = m_jsEngine.evaluate(str);
//   qDebug() << str << ret.toInt();
    int r = ret.toInt();
//    qDebug() << "PARSER "<<r;
    if (forceType==TokenType::ADDRESS && Syntax::s.m_currentSystem->m_system == AbstractSystem::OK64)
        return r;
    if (forceType==TokenType::BYTE)
        return r&0xFF;
    if (forceType==TokenType::INTEGER || forceType==TokenType::INTEGER_CONST || forceType==TokenType::ADDRESS)
        return r&0xFFFF;

    return ret.toInt();
}


int Parser::GetParsedIntOld()
{
    bool done = false;
    int val = 0;
    QString op = "plus";
/*    if (m_currentToken.m_value!="") {
        val= getIntVal(m_currentToken);
        Eat();
        return val;
    }
*/
    while (!done) {
//        qDebug() << "ival:"  << QString::number(m_currentToken.m_intVal);
        if (op == "plus")
            val = val + getParsedNumberOrConstant();
        if (op == "mul")
            val = val * getParsedNumberOrConstant();
        if (op == "div")
            val = val / getParsedNumberOrConstant();
        if (op == "minus")
            val = val - getParsedNumberOrConstant();


        Eat();
        done = true;
        if (m_currentToken.m_type == TokenType::MUL) {
            done = false;
            op = "mul";
        }
        if (m_currentToken.m_type == TokenType::DIV) {
            done = false;
            op = "div";
        }
        if (m_currentToken.m_type == TokenType::MINUS) {
            done = false;
            op = "minus";
        }
        if (m_currentToken.m_type == TokenType::PLUS) {
            done = false;
            op = "plus";
        }
        if (!done)
            Eat();

    }
//    qDebug() << QString::number(val);
//    qDebug() << "End " << Util::numToHex(val);

    return val;
}

int Parser::getIntVal(Token t)
{
    int val = t.m_intVal;
    if (t.m_value!="") {
        QSharedPointer<Symbol> s = m_symTab->Lookup(t.m_value,t.m_lineNumber);
        if (s!=nullptr)
            return s->m_value->m_fVal;
    }

    return val;
}

int Parser::findPage()
{
    int forcePage = 0;

    if (m_currentToken.m_type==TokenType::OFFPAGE) {
        forcePage = 1;
        Eat();
    }
    else
    if (m_currentToken.m_type==TokenType::ONPAGE) {
        forcePage = 2;
        Eat();
    }

    return forcePage ;
}

void Parser::VerifyTypeSpec(Token& t)
{
    if (m_isTRU) {
        if (t.m_value.startsWith(m_symTab->m_gPrefix))
            t.m_value.remove(m_symTab->m_gPrefix);
    }
    if (Syntax::s.keywords.contains(m_symTab->strip(t.m_value).toLower())) {
        ErrorHandler::e.Error("'"+m_symTab->strip(t.m_value).toLower()+"' is a reserved keyword, not a type.",m_currentToken.m_lineNumber);
    }
    try {

        bool ok = false;
        if (m_symTab->m_records.contains(t.m_value))
            ok=true;
        // What about
        if (m_symTab->m_records.contains(m_symTab->m_gPrefix+t.m_value)) {
            t.m_value = m_symTab->m_gPrefix+t.m_value;
            ok=true;
        }

        if (m_symTab->m_records.contains(m_symTab->m_currentUnit+t.m_value)) {
            t.m_value = m_symTab->m_currentUnit+t.m_value;
            ok=true;
        }

/*        QString test = m_+t.m_value;
        if (m_symTab->m_records.contains(test)) {
            t.m_value = test;
            ok=true;
        }
*/
//        qDebug() << "IS OK : "<<t.m_value << ok;

        if (!ok)
        if (!Syntax::s.m_currentSystem->m_allowedBaseTypes.contains(t.m_value)) {
            QString val = t.m_value;
//            ErrorHandler::e.Error("Unknown type : "+t.m_value, m_currentToken.m_lineNumber);
                QString similarSymbol = m_symTab->findSimilarSymbol(val,65,2,m_procedures.keys());
                QString em = "Unknown or illegal type : '"+val.remove(m_symTab->m_gPrefix)+"'. ";
                if (similarSymbol!="") {
                    em+="Did you mean '<font color=\"#A080FF\">"+similarSymbol+"</font>'?<br>";
                }
                qDebug() << "HERE "<<t.m_value;

                ErrorHandler::e.Error(em,m_currentToken.m_lineNumber);
        }

        //return;
/*        if (!ok)
            m_symTab->Lookup(t.m_value,t.m_lineNumber);
*/
    } catch (FatalErrorException& fe) {
        QString val = t.m_value;
//            ErrorHandler::e.Error("Unknown type : "+t.m_value, m_currentToken.m_lineNumber);
            QString similarSymbol = m_symTab->findSimilarSymbol(val,65,2,m_procedures.keys());
            QString em = "Unknown or illegal type : '"+val.remove(m_symTab->m_gPrefix)+"'. ";
            if (similarSymbol!="") {
                em+="Did you mean '<font color=\"#A080FF\">"+similarSymbol+"</font>'?<br>";
            }

            ErrorHandler::e.Error(em,m_currentToken.m_lineNumber);
  //      qDebug() <<m_symTab->m_records.keys() <<m_symTab->m_gPrefix<<t.m_value;
        throw fe;
    }
}

void Parser::RemoveUnusedProcedures()
{
    QString removeProcedures = "Removing unused procedures: ";
    bool outputUnusedWarning = false;
    QVector<QSharedPointer<Node>> procs;

/*    m_proceduresOnly.append(m_mergedProcedures);
    for (QSharedPointer<Node> n:m_proceduresOnly) {
        qDebug() << qSharedPointerDynamicCast<NodeProcedureDecl>(n)->m_procName;
    }*/
    // 6 passes should be enough... hopefully.
    for (int i=0;i<6;i++)
    for (QSharedPointer<Node> n: m_proceduresOnly) {
        QSharedPointer<NodeProcedureDecl> np = qSharedPointerDynamicCast<NodeProcedureDecl>(n);

        bool isUsed = np->m_isUsed;
        // Make sure none of the parents are unused!
        if (isUsed) {
            // Only optimize away non-interrupts
//            qDebug() << "TESTING " <<np->m_procName <<np->m_isUsedBy;
//            if (np->m_type==0 && !np->m_isUsedBy.contains("main")) {
                if (!np->m_isUsedBy.contains("main") && !m_doNotRemoveMethods.contains(np->m_procName)) {
                isUsed = false; // Assume not used after all
                for (auto s : np->m_isUsedBy) {
                    for (auto n2: m_proceduresOnly) {
                        auto np2 = qSharedPointerDynamicCast<NodeProcedureDecl>(n2);
                        if (np2->m_procName == s) {
                            if (np2->m_isUsed)
                                isUsed = true;
                        }
                    }

                    if (m_doNotRemoveMethods.contains(s))
                        isUsed = true;

                }
 //               qDebug() << np->m_procName << isUsed << n->m_isUsedBy;
            }
        }
        np->m_isUsed = isUsed;

    }


    for (QSharedPointer<Node> n: m_proceduresOnly) {
        QSharedPointer<NodeProcedureDecl> np = qSharedPointerDynamicCast<NodeProcedureDecl>(n);
        bool isUsed = np->m_isUsed;


        if ((isUsed==true) || m_doNotRemoveMethods.contains(np->m_procName))
            procs.append(n);
        else {
            outputUnusedWarning = true;
            removeProcedures+=np->m_procName + ",";
            m_removedProcedures << np->m_procName;
 //           qDebug() << "REMOVING procedure " <<np->m_procName << np->m_isUsedBy;
            //            if (m_procedures.contains(np->m_procName))
            //              m_procedures.remove(np->m_procName);
        }
    }

    m_proceduresOnly = procs;

    if (outputUnusedWarning) {
        removeProcedures.remove(removeProcedures.count()-1,1);
//        ErrorHandler::e.Warning(removeProcedures);
    }


}

void Parser::RemoveUnusedSymbols(QSharedPointer<NodeProgram> root)
{
    QVector<QSharedPointer<Node>> newDecl;
    QStringList removedSymbols;
    for (QSharedPointer<Node> n: root->m_NodeBlock->m_decl) {
        bool add = true;

        QSharedPointer<NodeVarDecl> nv = qSharedPointerDynamicCast<NodeVarDecl>(n);
        if (nv!=nullptr) {
            QString val = qSharedPointerDynamicCast<NodeVar>(nv->m_varNode)->value;

            if (m_symTab->m_symbols.contains(val) && !m_doNotRemoveMethods.contains(val)) {

                QSharedPointer<Symbol> s = m_symTab->m_symbols[val];//m_symTab->Lookup(,0);
//                qDebug() << s->m_type;
                bool isUsed = s->m_doNotOptimize;

                if (s->isUsed) {
                    for (QString p: m_removedProcedures)
                        if (s->isUsedBy.contains(p))
                            s->isUsedBy.removeAll(p);
                    if (s->isUsedBy.count()>0)
                        isUsed = true;


 //                   if (isUsed)

                }
//                qDebug() << "Used: " << s->m_name << isUsed <<s->isUsedBy;

                if (!isUsed && !(s->m_type=="INCBIN" || s->m_type=="INCSID")) {
                    removedSymbols.append(val);
                    m_symTab->m_symbols.remove(val);
             //       qDebug() << "REMOVING " << s->m_name << s->isUsed;
                    add = false;
                }

            }
            else add=true;
        }
        if (add)
            newDecl.append(n);
    }
    root->m_NodeBlock->m_decl = newDecl;
    if (removedSymbols.count()!=0) {
        QString s = "Removed the following unused symbols: ";
        for (int i=0;i<removedSymbols.count();i++) {
            s = s + removedSymbols[i];
            if (i!=removedSymbols.count()-1)
                s=s+", ";
        }
//        ErrorHandler::e.Warning(s);
    }

}


void Parser::HandlePreprocessorInParsing()
{
    if (m_pass>=PASS_CODE) {

        if (m_currentToken.m_value=="define") {
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="splitfile") {
            Eat();

            Eat();
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="setvalue") {
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="addmonitorcommand") {
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="compile_akg_music") {
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="deletefile") {
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="compress") {
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="execute") {
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="export") {
            Eat();
            Eat();
            Eat();
            Eat();
            if (m_currentToken.m_type==TokenType::INTEGER_CONST)
                Eat();
            return;
        }
        if (m_currentToken.m_value=="setcompressionweights") {
            Eat();
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="export_parallax_data") {
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            return;
        }

        if (m_currentToken.m_value=="perlinnoise") {
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="exportblackwhite") {
            Eat();
            Eat();
            Eat();
            while (m_currentToken.m_type==TokenType::INTEGER_CONST)
                Eat();
            return;
        }
        if (m_currentToken.m_value=="exportcompressed") {
            Eat();
            Eat();
            Eat();
            while (m_currentToken.m_type==TokenType::INTEGER_CONST)
                Eat();
            return;
        }
        if (m_currentToken.m_value=="exportrgb8palette") {
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="exportprg2bin") {
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value.toLower()=="vicmemoryconfig") {
            Eat();
            Eat();
        }
        if (m_currentToken.m_value.toLower()=="macro") {
            Eat();
            Eat();
            Eat();
            Eat();
        }
        if (m_currentToken.m_value=="spritepacker") {
            Eat();
            Eat(TokenType::STRING);
            Eat(TokenType::STRING);
            Eat(TokenType::STRING);
            Eat(TokenType::STRING);
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="importchar") {
            Eat();
            Eat();
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="spritecompiler") {
            Eat(); // Preprocessor
            Eat(); // Filename
            Eat(); // Name
            Eat(); // X
            Eat(); // Y
            Eat(); // W
            Eat(); // H
        }

        if (m_currentToken.m_value=="pbmexport") {
            Eat();
            Eat(TokenType::STRING);
            Eat(TokenType::STRING);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            return;
        }

        if (m_currentToken.m_value=="vbmexport") {
            Eat();
            Eat(TokenType::STRING);
            Eat(TokenType::STRING);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            return;
        }
        if (m_currentToken.m_value=="vbmexportcolor") {
            Eat();
            Eat(TokenType::STRING);
            Eat(TokenType::STRING);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            return;
        }

        if (m_currentToken.m_value=="vbmexportchunk") {
            Eat();
            Eat(TokenType::STRING);
            Eat(TokenType::STRING);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            return;
        }
        if (m_currentToken.m_value=="ignoresystemheaders") {
            Eat();
        }

        if (m_currentToken.m_value=="exportframe") {
            Eat();
            Eat(TokenType::STRING);
            Eat(TokenType::STRING);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            Eat(TokenType::INTEGER_CONST);
            return;
        }


        if (m_currentToken.m_value=="donotremove") {
            Eat();
            m_doNotRemoveMethods.append(m_symTab->m_gPrefix+m_currentToken.m_value);
            Eat();
            return;
        }
        if (m_currentToken.m_value=="userdata") {
            Eat();
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="requirefile") {
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="raisewarning") {
            Eat();
            if (!m_ignoreAll)
                ErrorHandler::e.Warning(m_currentToken.m_value, m_currentToken.m_lineNumber);
            //        Eat();
            Eat();
            return;
        }

        if (m_currentToken.m_value=="raiseerror") {
            Eat();
            if (!m_ignoreAll)
                ErrorHandler::e.Error(m_currentToken.m_value, m_currentToken.m_lineNumber);


            return;
        }

        if (m_currentToken.m_value=="error") {
                Eat();

                if (!m_ignoreAll)
                  ErrorHandler::e.Error(m_currentToken.m_value, m_currentToken.m_lineNumber);

                return;
        }

        if (m_currentToken.m_value=="ignoremethod") {
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="projectsettings") {
            Eat();
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="buildpaw") {
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="startassembler") {
            Eat();
            Eat();
            return;
        }
        if (m_currentToken.m_value=="use") {
            Eat();
            Eat();
            //        Eat();
            //        Eat();
            return;
        }


        if (m_currentToken.m_value=="include") {
            Eat();
            Eat();
            return;
        }
    }

//    qDebug() <<"VAL " <<m_currentToken.m_value;

    if (!m_ignoreAll) {
        if (m_currentToken.m_value=="ifdef" || m_currentToken.m_value=="if") {
            PreprocessIfDefs(true);
            return;
        }
        if (m_currentToken.m_value=="ifndef") {
            PreprocessIfDefs(false);
            return;
        }

        if (m_currentToken.m_value=="else") {
            //        qDebug() << "Start with ELSE : " << m_lastIfdef.last() << m_lastKey.last();
            PreprocessIfDefs(!m_lastIfdef.last());
            return;
        }
        if (m_currentToken.m_value=="endif") {

//            qDebug() << "ENDIF " <<m_lastKey;

            if (m_lastKey.count()==0)
                ErrorHandler::e.Error("Preprocessor '@endif' mismatch error", m_currentToken.m_lineNumber);

            m_lastKey.removeLast();
            m_lastIfdef.removeLast();
            Eat();
            return;
        }


    }



    if (m_currentToken.m_value=="endblock") {
        int i = m_pass;
        if (Node::m_staticBlockInfo.m_blockID ==-1) {
            ErrorHandler::e.Error("Cannot end a block that hasn't been started.",m_currentToken.m_lineNumber);
        }
        m_pass = PASS_OTHER;
        Eat();
//        if (i == PASS_CODE)
  //          qDebug() << "HandleCurrent Endblock " << Node::m_staticBlockInfo.m_blockPos;
        m_pass = i;
        Node::m_staticBlockInfo.m_blockID = -1;
        Node::m_staticBlockInfo.m_blockPos = "";
        Node::m_staticBlockInfo.m_blockName="";
        m_pass = i;
        return;
    }
    if (m_currentToken.m_value=="startblock") {
        int i = m_pass;
/*        qDebug() << Node::m_staticBlockInfo.m_blockID;
        qDebug() << Node::m_staticBlockInfo.m_blockPos;
        qDebug() << Node::m_staticBlockInfo.m_blockName;*/
        if (Node::m_staticBlockInfo.m_blockID !=-1) {
//            ErrorHandler::e.Error("Cannot start a block without ending the previous. ",m_currentToken.m_lineNumber);
        }
        m_pass = PASS_OTHER;
        Eat();
        QString startPos = m_currentToken.getNumAsHexString();
        Eat();
        QString name = m_currentToken.m_value;
        m_lastStartBlockToken = m_currentToken;
        Eat();
        ParserBlock pb;
        pb.m_blockID = m_parserBlocks.count();
        pb.pos = startPos;
        m_parserBlocks.append(pb);
        Node::m_staticBlockInfo.m_blockID = pb.m_blockID;
        Node::m_staticBlockInfo.m_blockPos = pb.pos;
        Node::m_staticBlockInfo.m_blockName = name;
        m_pass = i;

      //  if (i == PASS_CODE)
    //    qDebug() << "HandleCurrent StartBlock " << Node::m_staticBlockInfo.m_blockPos;

        return;
    }
    if (m_macros.contains(m_currentToken.m_value.toLower())) {
        HandleCallMacro(m_currentToken.m_value.toLower(), true);
    }


}

void Parser::StripWhiteSpaceBeforeParenthesis()
{
    m_lexer->m_text = m_lexer->m_text.replace(QRegularExpression("\\s*(\\()"),"\\1");
    m_lexer->m_text = m_lexer->m_text.replace("sta(", "sta (");
}

void Parser::RemoveComments()
{
    QRegularExpression rg = QRegularExpression("/\\*([^*]|[\\r\\n]|(\\*+([^*/]|[\\r\\n])))*\\*+/");

    //qDebug() << rg;
    m_lexer->m_text = m_lexer->m_text.replace(rg, "");

    QRegularExpression rg2 = QRegularExpression("//.*?\\n");
    //qDebug() << rg;
    m_lexer->m_text = m_lexer->m_text.replace(rg2, "");

    //qDebug() << m_lexer->m_text;

}

bool Parser::PreprocessIncludeFiles()
{
    m_lexer->Initialize();
    m_lexer->m_ignorePreprocessor = false;
    m_acc = 0;
    bool done = true;
    m_currentToken = m_lexer->GetNextToken();
    //m_preprocessorDefines.clear();
    while (m_currentToken.m_type!=TokenType::TEOF) {
        if (m_currentToken.m_type == TokenType::PREPROCESSOR) {
            if (m_currentToken.m_value.toLower()=="include") {
                Eat(TokenType::PREPROCESSOR);
                QString name = m_currentToken.m_value;
  //              qDebug() << "INCLUDING " <<name;

                // First, check the local dir
                QString filename =(m_currentDir +"/"+ m_currentToken.m_value);
                filename = filename.replace("//","/");
                // Then, if the file doesn't exist, check unit SYSTEM dir
                // BORK EVERYTHING if current stuff is a TRU
                //if (m_isTRU) filename="";
                if (!QFile::exists(filename))
                    filename =Util::path +Data::data.unitPath +QDir::separator()+AbstractSystem::StringFromSystem(Syntax::s.m_currentSystem->m_system)+QDir::separator()+ m_currentToken.m_value;

                // Then, if the file doesn't exist, check unit PROCESSOR dir
                if (!QFile::exists(filename))
                    filename =Util::path +Data::data.unitPath + QDir::separator() +"cpu_specific" +QDir::separator()+AbstractSystem::StringFromProcessor(Syntax::s.m_currentSystem->m_processor)+QDir::separator()+ m_currentToken.m_value;


                QString text = m_lexer->loadTextFile(filename);
                int ln=m_lexer->getLineNumber(m_currentToken.m_value)+m_acc;
                m_lexer->m_text.insert(m_lexer->m_pos, text);
                int count = text.split("\n").count();
                m_lexer->m_includeFiles.append(
                            FilePart(name,ln, ln+ count, ln-m_acc,ln+count-m_acc,count));
                m_acc-=count-1;
                done = false;
                Eat(TokenType::STRING);            }
        }
        Eat();
//        qDebug() << m_currentToken.m_value;

    }
    return done;
  //  qDebug() <<m_lexer->m_text;
}



QSharedPointer<Node> Parser::Variable(bool isSubVar)
{
    QSharedPointer<Node> n = nullptr;

    bool isConstant = false;
    bool isRegister = m_symTab->isRegisterName(m_currentToken.m_value);

//    qDebug() << "IS REGISTER: "<< m_currentToken.m_value << isRegister;
//    qDebug() << "SUBVAR  "<< isSubVar << m_currentToken.m_value;

    if (m_currentToken.m_value == Syntax::s.thisName)
        m_currentToken.m_value = m_currentClass+"_"+m_currentToken.m_value;

    m_currentToken.m_value = VerifyVariableName(m_currentToken.m_value);
    // Rename "i" with "_var_i" for disallowed variables (Z80, GB)

    // Subvar can't be CONST
    if (!isSubVar && !isRegister && m_symTab->m_constants.contains(m_currentToken.m_value.toUpper())) {
        QSharedPointer<Symbol> s = m_symTab->m_constants[m_currentToken.m_value.toUpper()];
        isConstant=true;

//        qDebug() << "PARSER looking for " << m_currentToken.m_value << " , found symbol: " << s->m_name << " with value " << s->m_value->m_fVal << " of type " << s->m_type;

//        qDebug() << m_currentToken.m_value;
//        qDebug() << s->m_value->m_fVal << 0xffff8240;
//        qDebug() << "PHERE "<< Util::numToHex(s->m_value->m_fVal);



        if (s->m_type=="ADDRESS") m_currentToken.m_type=TokenType::ADDRESS;
        if (s->m_type=="LONG") m_currentToken.m_type=TokenType::LONG;
        if (s->m_type=="INTEGER") m_currentToken.m_type=TokenType::INTEGER;
        if (s->m_type=="WORD") m_currentToken.m_type=TokenType::INTEGER;
        if (s->m_type=="BYTE") m_currentToken.m_type=TokenType::BYTE;
        if (s->m_type=="BOOLEAN") m_currentToken.m_type=TokenType::BYTE;
        if (s->m_type=="STRING") m_currentToken.m_type=TokenType::STRING;
        if (s->m_type=="CSTRING") m_currentToken.m_type=TokenType::CSTRING;

        Token t = m_currentToken;
        Eat(m_currentToken.m_type);
        QSharedPointer<Node> expr = nullptr;
        if (m_currentToken.m_type==TokenType::LBRACKET) {
            Eat(TokenType::LBRACKET);
            QString org = m_symTab->m_gPrefix;
            //qDebug() << "PARSER " <<org;
 //           if (isSubVar && !m_symTab->m_gPrefix.startsWith("localVariable"))
                m_symTab->m_gPrefix ="";
            expr = Expr();
            m_symTab->m_gPrefix = org;
            Eat(TokenType::RBRACKET);
         }
        QSharedPointer<Node> subVar = nullptr;
        if (m_currentToken.m_type==TokenType::DOT) {
            Eat();
            subVar = SubVariable(t.m_value, nullptr);
/*            if (m_breakSubvar) {
                // Is procedure
                m_breakSubvar=false;
                return subVar;
            }
            */
        }

        if (t.m_type==TokenType::ADDRESS && expr!=nullptr) {
            t.m_value = "$"+QString::number( (long)s->m_value->m_fVal,16);
            QSharedPointer<NodeVar> nv = QSharedPointer<NodeVar>(new NodeVar(t,expr));
            nv->m_subNode = subVar;
            //nv->m_expr = expr;
            n=nv;
        }
        else {
            n = QSharedPointer<NodeNumber>(new NodeNumber(t, s->m_value->m_fVal));

            //qDebug()  << s->m_value->m_fVal;
        }


    }
    else {

        Token t = m_currentToken;
        if (m_currentToken.m_type==TokenType::STRING || m_currentToken.m_type==TokenType::CSTRING) {
           n = String(m_currentToken.m_type==TokenType::CSTRING);
           return n;

        }
        // Make sure that prefixes are added
//        qDebug() << "PREFIX " << m_symTab->m_globalList.contains(t.m_value) << t.m_value <<  m_symTab->m_gPrefix+t.m_value ;
  //     qDebug() << m_symTab->m_globalList;
//        qDebug() << "IS REGISTER " <<t.m_value << isRegister;
        if (!isRegister)
            if (!m_symTab->m_globalList.contains(t.m_value))
 //               if (!isRegister)
                   t.m_value = m_symTab->m_gPrefix+t.m_value;



       // qDebug() << "PARSER2: " <<t.m_value << m_symTab->m_gPrefix <<m_symTab->m_globalList.contains(t.m_value) << m_symTab->m_gPrefix;
  //      qDebug() <<


        Eat(m_currentToken.m_type);

        if (m_currentToken.m_type!=TokenType::LBRACKET) {
            if (t.m_value.endsWith("^")) {
                t.m_value.replace("^","");
  //              qDebug() << t.m_value;
//                exit(1);
                t.m_type = TokenType::ADDRESS;
            }
            QSharedPointer<Node> subVar = nullptr;
            if (m_currentToken.m_type==TokenType::DOT) {
                Eat();
                subVar = SubVariable(t.m_value, nullptr);
/*                if (m_breakSubvar) {
                    // Is procedure
                    m_breakSubvar=false;
                    return subVar;
                }
                */


            }

            n = QSharedPointer<NodeVar>(new NodeVar(t));
            qSharedPointerDynamicCast<NodeVar>(n)->m_subNode = subVar;
        }
        else
            {
                Eat(TokenType::LBRACKET);
                //QSharedPointer<Node> expr = Expr();
                QString org = m_symTab->m_gPrefix;
                if (isSubVar)
                    m_symTab->m_gPrefix ="";

                auto expr = Expr();
                m_symTab->m_gPrefix = org;
                Eat(TokenType::RBRACKET);



                QSharedPointer<Node> subVar = nullptr;
                if (m_currentToken.m_type==TokenType::DOT) {
                    Eat();
//                    qDebug() << "H1";
                    subVar = SubVariable(t.m_value, expr);
/*                    if (m_breakSubvar) {
                        // Is procedure
                        m_breakSubvar=false;
                        return subVar;
                    }
                    */

  //                  qDebug() << "H2";
                }
                n = QSharedPointer<NodeVar>(new NodeVar(t, expr));
                qSharedPointerDynamicCast<NodeVar>(n)->m_subNode = subVar;
                QSharedPointer<Symbol> s = m_symTab->Lookup(qSharedPointerDynamicCast<NodeVar>(n)->value,m_currentToken.m_lineNumber);

                if (expr!=nullptr && t.m_isReference && !m_symTab->m_records.contains(s->getEndType())) {
                    // change  arr[i] to #arr +i
                    qSharedPointerDynamicCast<NodeVar>(n)->m_expr = nullptr;
                    Token tb;
                    tb.m_type = TokenType::PLUS;
                    tb.m_lineNumber = t.m_lineNumber;
                    QSharedPointer<Node> expr2 = expr;
                    int size = 1;
                    if (s->m_arrayType==TokenType::INTEGER) size = 2;
                    if (s->m_arrayType==TokenType::LONG) size = 4;
                    //qDebug() << "PARSER SIZE" << size;
                    if (size!=1) {
                        //expr2 will also become a nodebinop
                        Token tb2;
                        tb2.m_type = TokenType::SHL; // Shift left
                        tb2.m_lineNumber = t.m_lineNumber;

                        Token tn;
                        tn.m_type = TokenType::INTEGER_CONST; // Shift left
                        tn.m_lineNumber = t.m_lineNumber;
                        int s = 1;
                        if (size == 4) s = 2; // Shift value
                        tn.m_intVal = s;
                        expr2 = QSharedPointer<NodeBinOP>(new NodeBinOP(expr,tb2,QSharedPointer<NodeNumber>(new NodeNumber(tn,s))));
                    }
                    QSharedPointer<Node> nb = QSharedPointer<NodeBinOP>(new NodeBinOP(n,tb,expr2));
                    n = nb;
                }


        }

    }
    if (n==nullptr) {
        ErrorHandler::e.Error("Could not assign variable!");
    }
/*    QSharedPointer<NodeVar> nv = qSharedPointerDynamicCast<NodeVar>n;
    if (nv->m_subNode!=nullptr && (qSharedPointerDynamicCast<NodeVar>nv->m_subNode)->m_expr!=nullptr)
        nv->m_expr = (qSharedPointerDynamicCast<NodeVar>nv->m_subNode)->m_expr;
*/
    // Verify that we're not trying to screw with the variable

    QSharedPointer<NodeVar> nv = qSharedPointerDynamicCast<NodeVar>(n);
//    if (!m_ignoreLookup)
    if (nv!=nullptr) {

        if (isRegister) {
            nv->m_isRegister = true;
            return n;
        }

        QSharedPointer<Symbol> s = m_symTab->Lookup(nv->value,m_currentToken.m_lineNumber);
        // If variable doesn't exist
//        qDebug
        if (s==nullptr) {
            ErrorHandler::e.Error("Could not find variable : " +nv->value);
        }
//        qDebug() << nv->value<<s->m_type;
        if (!(s->m_type.toUpper()=="ARRAY" || s->m_type.toUpper()=="POINTER" || s->m_type.toUpper()=="STRING"  ||s->m_type.toUpper()=="CSTRING" ||s->m_type.toUpper()=="INCBIN" || s->m_type.toUpper()=="ADDRESS" || isConstant) && nv->m_expr!=nullptr)
            ErrorHandler::e.Error("Variable '<b>" +nv->value + "</b>' is neither a pointer nor an array.",nv->m_op.m_lineNumber);

    }

    return ApplyClassVariable(n);
}

QSharedPointer<Node> Parser::SubVariable(QString parent,QSharedPointer<Node> parentExpr = nullptr)
{
    QString fix = m_symTab->m_gPrefix;
 //   qDebug() << "SUBVAR ORG " <<fix << parent << m_symTab->m_symbols.keys();
    QSharedPointer<Symbol> s = m_symTab->Lookup(parent,m_currentToken.m_lineNumber);
//    qDebug() << "SUBVAR TYPE " <<s->m_arrayTypeText << parent;
    QString recordType = s->getEndType();

    if (m_symTab->m_records.contains(recordType))
    if (m_symTab->m_records[recordType]->m_isClass) {
        QString procName = recordType+"_" +m_currentToken.m_value;
        // OOps! Are we calling a method?
        if (m_procedures.contains(procName)) {
            m_currentToken.m_value = procName;
            bool isAssign;
//            qDebug() << "A";
            // Make sure that the FIRST parameter is a reference to the parent!
//            parent = parent.replace("ull","pu");
            m_addInitialReferenceToProcedureCall = parent;
            QString keep = m_symTab->m_gPrefix;
            m_symTab->m_gPrefix = "";
 //           qDebug() << "Testing for procname " <<procName<< parent<<parentExpr<<m_symTab->m_gPrefix;
            QSharedPointer<Node> node = FindProcedure(isAssign, parentExpr);
            m_symTab->m_gPrefix = keep;
   //         qDebug() <<m_procedures.keys() <<node;
            m_addInitialReferenceToProcedureCall = "";
//            m_breakSubvar = true;
//            qDebug() << "Found subvar procedure "<<procName;
  //          qDebug() << "B" <<node ;
            m_currentProcedureCall = node;
//            if (node!=nullptr)
  //              return node;
//            qDebug() << "WE ARE DONE?";
            return nullptr;//Empty();
            // YES we are!

        }
    }
    if (recordType.toLower()=="pointer")
        recordType = s->m_pointsTo;
    if (recordType.toLower()=="array")
        recordType = s->m_arrayTypeText;



    m_symTab->m_gPrefix = recordType+"_";
    QSharedPointer<Node> n = Variable(true);
    m_symTab->m_gPrefix = fix;
  //  qDebug() << "DONE";
    return n;
}

QSharedPointer<Node> Parser::Empty()
{
    return QSharedPointer<Node>(new NoOp());
}


QVector<QSharedPointer<Node>> Parser::Record(QString name)
{
    QVector<QSharedPointer<Node>> decls;
    bool isClass = m_currentToken.m_type==TokenType::CLASS;
    Eat();
//    SymbolTable
    QSharedPointer<SymbolTable>  record = QSharedPointer<SymbolTable> (new SymbolTable());
    record->m_symbols.clear();
    m_symTab->m_records[name] = record;
    //m_symTab->Define(new Symbol(name,"record"));
    record->setName(name);
    QString oldPrefix = m_symTab->m_gPrefix;
    m_symTab->m_gPrefix=name+"_";
    m_isRecord = true;
    bool first = true;

    if (isClass) {
        QString of = m_symTab->m_gPrefix;
//        if (!m_symTab->m_gPrefix.startsWith("localVariable"))
            m_symTab->m_gPrefix = "";
        auto s = QSharedPointer<Symbol>(new Symbol(name+"_"+Syntax::s.thisName,name));
        m_symTab->m_gPrefix = of;
        m_symTab->Define(s,true);
        s->m_doNotOptimize = true;

    }
    record->m_isClass = isClass;

    while (m_currentToken.m_type!=TokenType::END) {


        if (isClass) {
            m_currentClass = name;
            if (m_currentToken.m_type==TokenType::PROCEDURE) {
                QVector<QSharedPointer<Node>> procs;

                QString of = m_symTab->m_gPrefix;
                m_symTab->m_gPrefix = "";
                m_procPrefix = of;
                ProcDeclarations(procs,"");
                m_symTab->m_gPrefix = of;
                m_procPrefix = "";


                for (auto& n:procs) {
                    auto p = qSharedPointerDynamicCast<NodeProcedureDecl>(n);
                    p->m_class = name;
                    p->m_isStatic = false;
                    // First parameter to *always* point to the current object
                    Token varToken = m_currentToken;
                    varToken.m_type = TokenType::ID;
                    varToken.m_value = name+"_"+Syntax::s.thisName;
                    Token typeToken = m_currentToken;
                    typeToken.m_type = TokenType::POINTER;
                    typeToken.m_value = "POINTER";
//                    typeToken.
                    auto var = QSharedPointer<NodeVar>(new NodeVar(varToken));
                    auto type = QSharedPointer<NodeVarType>(new NodeVarType(typeToken,""));
                    type->m_arrayVarType.m_type = TokenType::POINTER;
                    type->m_arrayVarType.m_value = name;

                    if (!first)
                        type->m_flags<<"global"; // Make "this" global on 2nd+ params


                    auto paramSelf = QSharedPointer<NodeVarDecl>(new NodeVarDecl(var,type));

                    p->m_paramDecl.insert(0,paramSelf);
                    if (first)
                       decls.append(paramSelf);
                    first = false;
   //                 qDebug() << "HERE" << name << paramSelf<<p->m_paramDecl.count();

 //                   p->m_paramDecl.insert(0,paramSelf);
                }

            }

        }
        if (m_currentToken.m_type!=TokenType::END && m_currentToken.m_type!=TokenType::PROCEDURE && m_currentToken.m_type!=TokenType::FUNCTION) {
//            qDebug() << "A "<<m_symTab->m_gPrefix;
            QVector<QSharedPointer<Node>> vars = VariableDeclarations("");
            for (QSharedPointer<Node> n : vars) {
                QSharedPointer<NodeVarDecl> nv = qSharedPointerDynamicCast<NodeVarDecl>(n);
                QSharedPointer<NodeVarType> typ = qSharedPointerDynamicCast<NodeVarType>(nv->m_typeNode);
                QSharedPointer<NodeVar> var = qSharedPointerDynamicCast<NodeVar>(nv->m_varNode);
                QSharedPointer<Symbol> s = QSharedPointer<Symbol>(new Symbol(var->value, typ->value));
                s->m_arrayType = typ->m_arrayVarType.m_type;
                s->m_arrayTypeText = typ->m_arrayVarType.m_value;
                s->m_flags = typ->m_flags;
                s->m_type = typ->m_op.m_value;

                record->Define(s);
                s->setSizeFromCountOfData(typ->m_declaredCount);
//                qDebug() << s->m_name << s->m_size << s->m_arrayTypeText << s->m_type;
                record->m_orderedByDefinition.append(var->value);
//                n->ExecuteSym(m_symTab);
            }
            Eat(TokenType::SEMI);
        }

    }
//    qDebug() << "************ PARSER Calculating size "<<name <<record->getSize();;

    SymbolTable::s_classSizes[name] = record->getSize();
//    qDebug() << "Size: " <<name<<SymbolTable::s_classSizes[name];
    m_currentClass = "";
    m_isRecord = false;
//    m_symTab = oldTab;
    m_symTab->m_gPrefix = oldPrefix;

    Eat(TokenType::END);
//    Eat(TokenType::SEMI);
    return decls;
}

// Doesn't currently work. Think about it.

QSharedPointer<Node> Parser::AssignStatementBetweenObjects(QSharedPointer<Node> left, QSharedPointer<Node> right)
{
    QSharedPointer<NodeVar> var = qSharedPointerDynamicCast<NodeVar>(left);
    if (var!=nullptr  && var->m_subNode==nullptr && var->isPureObject) {
        QString t1,t2;
//        qDebug() << "HERE " << t2  <<var->value<<var->isRecord(m_symTab,t1)<<t1;
        if (var->isRecord(m_symTab,t1)) {// && m_symTab->m_records[t1]->m_isClass) {

            if (!right->isRecord(m_symTab,t2) || (t1!=t2))
                ErrorHandler::e.Error("Can only assign an object of a record/class to another object of the same type",left->m_op.m_lineNumber);

            QSharedPointer<NodeVar> varR = qSharedPointerDynamicCast<NodeVar>(right);
/*
            auto cpd = QSharedPointer<NodeCompound>(new NodeCompound(m_currentToken));
            Token t = m_currentToken;
            t.m_type = TokenType::ASSIGN;
            qDebug() << "HERE2 "<<t1 << t2  <<var->value;
            qDebug() << m_symTab->m_symbols.keys();
            for (auto& s: m_symTab->m_records[t1]->m_symbols) {
//                qDebug() << s->m_name;
                QString n = s->m_name;
              //  n = n.remove(t1+"_");
                var->m_subNode = CreateVariable(n);
                varR->m_subNode = CreateVariable(n);
                var = qSharedPointerDynamicCast<NodeVar>(ApplyClassVariable(var));
                varR = qSharedPointerDynamicCast<NodeVar>(ApplyClassVariable(varR));
                cpd->children.append(QSharedPointer<NodeAssign>(new
                                                                NodeAssign(var,t,varR)));
                qDebug() << "Appending "<<n;

//                left.m_ex
//                QSharedPointer<NodeAssign> na = QSharedPointer<NodeAssign>(new NodeAssign(left, t, right));

//                var->m_subNode
            }
            return cpd;
  */
             QVector<QSharedPointer<Node>> params;
            left->m_op.m_isReference = true;
            right->m_op.m_isReference = true;
            int size = m_symTab->m_records[t1]->getSize();
            if (Syntax::s.m_currentSystem->m_processor==AbstractSystem::MOS6502)
                params<<right<<CreateNumber(0)<<left<<CreateNumber(size);
            if (Syntax::s.m_currentSystem->m_processor==AbstractSystem::Z80)
                params<<right<<left<<CreateNumber(size);
            return QSharedPointer<NodeBuiltinMethod>(new NodeBuiltinMethod("memcpyunroll",params,&Syntax::s.builtInFunctions["memcpyunroll"]));
            // guaranteed that both are the same!
//                Node

                // Doesn't currently work
            }
    }
    return nullptr;
}


QSharedPointer<Node> Parser::AssignStatement()
{
    QSharedPointer<Node> arrayIndex = nullptr;
    m_currentProcedureCall = nullptr;
    Token t = m_currentToken;
    QSharedPointer<Node> left = Variable();

    if (t.m_isReference)
        ErrorHandler::e.Error("Assignment '"+t.m_value+"' cannot be a reference. " , t.m_lineNumber);


    // test if left is procedure after all
    if (qSharedPointerDynamicCast<NodeProcedure>(left)!=nullptr)
        return left; //is a procedure type m.Draw();


    if (m_currentProcedureCall!=nullptr) {
        // ooh we had a variable of type a.b.c.Move();
        auto p = qSharedPointerDynamicCast<NodeProcedure>(m_currentProcedureCall);
//        left->setForceReference(true);
        auto v = qSharedPointerDynamicCast<NodeVar>(left);
        // Transform to a reference..
        auto s =m_symTab->Lookup(v->value,m_currentToken.m_lineNumber);;
        //qDebug() << s->m_name << s->m_type << s->m_arrayTypeText <<s->m_pointsTo <<s->m_type.toLower();
        left = ApplyClassVariable(left);
        if (s->m_type.toLower()!="pointer")
            left->setReference(true);
        if (!left->isReference()) {
//            qDebug() << "IS REFERENCE ";
            if (v->m_expr!=nullptr) {
                auto add = v->m_expr;
                v->m_expr = nullptr;
                left = CreateBinop(TokenType::PLUS,v,add);

            }
        }

        p->m_parameters.insert(0,left);
        return p;

    }


    Token token = m_currentToken;

    if (m_currentToken.m_type!=TokenType::ASSIGN && m_currentToken.m_type!=TokenType::ASSIGNOP) {
//        qDebug() << m_currentToken;
        // First, check if similar procedure exists
        QString val = t.m_value;
        QString em = "Could not find procedure '<font color=\"#FF8000\">" + val + "</font>'<br>";
//        QString similarSymbol = m_symTab->findSimilarSymbol(val,70,2,m_procedures.keys());
        QString similarSymbol = "";
          for (QString s:m_procedures.keys()) {
                if (Util::QStringIsSimilar(val,s,65,2,Qt::CaseInsensitive)) {
                    similarSymbol = s;
                    break;
                }
            }

        if (similarSymbol!="") {
            em+="Did you mean '<font color=\"#A080FF\">"+similarSymbol+"</font>'?<br>";
            ErrorHandler::e.Error(em , token.m_lineNumber);
        }



        ErrorHandler::e.Error("Error assigning variable <b>'" + t.m_value+  "'</b>, did you forget a colon or mistype? Syntax should be: <b>'a := b;'</b>." , token.m_lineNumber);
    }


    if (m_currentToken.m_type==TokenType::ASSIGNOP) {
        // A += 2; etc
        QString op_command = m_currentToken.m_value;
        Eat(TokenType::ASSIGNOP);
        QSharedPointer<Node> right = Expr();
        Token op = m_currentToken;
        op.m_type = Token::getBinopTokenTypeFromString(op_command);
        QSharedPointer<NodeBinOP> binop = QSharedPointer<NodeBinOP>(new NodeBinOP(left, op,right));

        QSharedPointer<NodeAssign>na = QSharedPointer<NodeAssign>(new NodeAssign(left, t, binop));

        auto s = getSymbol(left);
        na->m_right->ApplyFlags();// make sure integer:=byte*byte works
        return na;

    }
    // Regular assign
    Eat(TokenType::ASSIGN);
    QSharedPointer<Node> right = Expr();

    // Test for object2 := object2;
    /*
     * Doesn't work. Bloody hard!

    auto n = AssignStatementBetweenObjects(left,right);
    if (n!=nullptr)
        return n;
    */
    QSharedPointer<NodeAssign>na = QSharedPointer<NodeAssign>(new NodeAssign(left, t, right));

    // Test for assigning records / classes : a := b; where both are records


    // Verify that assign statement is OK
    auto s = getSymbol(left);

    na->m_right->ApplyFlags();// make sure integer:=byte*byte works
    return na;


}

QSharedPointer<Node> Parser::Statement()
{
    QSharedPointer<Node> node = nullptr;
    if (m_currentToken.m_type == TokenType::BEGIN) {
        node = CompoundStatement();
    }
    else if (m_currentToken.m_type == TokenType::ID) {
        bool isAssign;
        node = FindProcedure(isAssign, nullptr);
        if (isAssign) {
            return Empty();
        }

        if (node==nullptr)
            node = BuiltinFunction();
//        if (node==nullptr)
//            node = Constant();
        if (node==nullptr)
            node = AssignStatement();


    }
    else if (m_currentToken.m_type == TokenType::ADDRESS) {
        if (node==nullptr)
            node = AssignStatement();

    }
    else if (m_currentToken.m_type == TokenType::IF) {
        Eat(TokenType::IF);
        node = Conditional();
    }
    else if (m_currentToken.m_type == TokenType::FORI) {
        node = ForLoop(true);
    }
    else if (m_currentToken.m_type == TokenType::FOR) {
        node = ForLoop(false);
    }
    else if (m_currentToken.m_type == TokenType::BREAK || m_currentToken.m_type == TokenType::CONTINUE || m_currentToken.m_type == TokenType::RETURN) {
        node = QSharedPointer<NodeControlStatement>(new NodeControlStatement(m_currentToken));
        Eat();
        if (m_currentToken.m_type==TokenType::LPAREN) {
            Eat(TokenType::LPAREN);
            Eat(TokenType::RPAREN);
        }
    }
    else if (m_currentToken.m_type == TokenType::WHILE) {
        Eat(TokenType::WHILE);
        node = Conditional(true);
    }
    else if (m_currentToken.m_type == TokenType::REPEAT) {
        node = RepeatUntil();
    }
    else if (m_currentToken.m_type == TokenType::ASM) {
        return InlineAssembler();

    } else if (m_currentToken.m_type == TokenType::CASE) {
        return Case();
    }

    else {
        //ErrorHandler::e.Error("Unknown method " + m_currentToken.getType());
        return Empty();
    }

    if (node==nullptr)
        ErrorHandler::e.Error("Node is nullpointer. Should not happen. Contact leuat@irio.co.uk and slap him.",0);

    AppendComment(node);

    return node;


}

QSharedPointer<Node> Parser::Case()
{
    QSharedPointer<NodeCase> n = QSharedPointer<NodeCase>(new NodeCase(m_currentToken));
    Eat(); // Eat "case"
    n->m_variable = qSharedPointerDynamicCast<NodeVar>(Variable());
    if (n->m_variable==nullptr)
        ErrorHandler::e.Error("Case statements only work with variables.", m_currentToken.m_lineNumber);
    Eat(TokenType::OF);
    while (m_currentToken.m_type != TokenType::END && m_currentToken.m_type != TokenType::ELSE) {
        QSharedPointer<Node> expr = Expr();
        Eat(TokenType::COLON);
        QSharedPointer<Node> block = Block(false);
        n->Append(expr, block);
        Eat(); // Eat the semicolon

    }
    if (m_currentToken.m_type == TokenType::ELSE) {
        Eat();
        n->m_elseBlock = Block(false);
    }
    else
    Eat(); // Eat final END
    return n;

}

QSharedPointer<Node> Parser::BinaryClause()
{
    if (m_currentToken.m_type == TokenType::LPAREN && !nextIsExpr()) {
        // Logical clause AND OR
        Eat(TokenType::LPAREN);
        QSharedPointer<Node> a = BinaryClause();
        if (m_currentToken.m_type==TokenType::RPAREN) {
            Eat();
            return a;
        }
        Token logical = m_currentToken;
        Eat();
        QSharedPointer<Node> b = BinaryClause();
        Eat(TokenType::RPAREN);
        return QSharedPointer<NodeBinaryClause>(new NodeBinaryClause(logical, a, b));
    }
    //qDebug() << "***** BEFORE EXPR";
    QSharedPointer<Node> a = Expr();
    //qDebug() << "***** AFTER EXPR";
    Token comparetoken;
    QSharedPointer<Node> b;
    // Nothing : the null test. Check if NOT EQUALS ZERO

    if (m_currentToken.m_type==TokenType::RPAREN
            || m_currentToken.m_type==TokenType::THEN
            || m_currentToken.m_type==TokenType::DO
            || m_currentToken.m_type==TokenType::AND
            || m_currentToken.m_type==TokenType::OR
            || m_currentToken.m_type==TokenType::SEMI
            || m_currentToken.m_type==TokenType::XOR
            || m_currentToken.m_type==TokenType::OFFPAGE
            || m_currentToken.m_type==TokenType::ONPAGE)  {
        Token t;
        t.m_type = TokenType::BYTE;
        t.m_intVal = 0;
        t.m_lineNumber = m_currentToken.m_lineNumber;
        t.m_isReference = false;
 //       qDebug() << "WOOT";
        b = QSharedPointer<NodeNumber>(new NodeNumber(t,0));
        comparetoken.m_type = TokenType::NOTEQUALS;
    }
    else
    {

        comparetoken = m_currentToken;

        if (!(comparetoken.m_type==TokenType::EQUALS || comparetoken.m_type==TokenType::NOTEQUALS ||
            comparetoken.m_type==TokenType::GREATER || comparetoken.m_type==TokenType::LESS ||
            comparetoken.m_type==TokenType::GREATEREQUAL || comparetoken.m_type==TokenType::LESSEQUAL))
        {
            ErrorHandler::e.Error("Unknown compare type : '" + comparetoken.m_value+"'. Did you mean '=' or '>' etc?",comparetoken.m_lineNumber);
        }



         Eat();
        b = Expr();

    }
    return QSharedPointer<NodeBinaryClause>(new NodeBinaryClause(comparetoken, a, b));
}

bool Parser::isRecord(Token &t)
{

    return m_symTab->m_records.contains(m_currentToken.m_value);

//    return false;

}

bool Parser::isClass(Token &t)
{
/*    qDebug() <<t.m_value;
    if (m_symTab->m_records.contains(t.m_value))
        qDebug() <<t.m_value <<isRecord(t)<<m_symTab->m_records[t.m_value]->m_isClass;
*/
    return isRecord(t) && m_symTab->m_records[t.m_value]->m_isClass;

}

bool Parser::nextIsExpr()
{
    bool ret = true;
    Token keep = m_currentToken;
    m_lexer->PushState();
//    qDebug() << "Trying EXPRESSION... ";
    try {
        QSharedPointer<Node> val = Expr();
    }
    catch(const FatalErrorException& fe) {
        ret = false;
    }
  //  qDebug() << "Next is EXPRESSION : " << ret;
    m_currentToken = keep;
    m_lexer->PopState();
    return ret;

}


void Parser::AppendComment(QSharedPointer<Node> n)
{
    if (m_lexer->m_currentComment=="")
        return;
    if (n==nullptr)
        return;
    n->m_comment = m_lexer->m_currentComment;
    m_lexer->m_currentComment="";
}



QSharedPointer<Node> Parser::Conditional(bool isWhileLoop)
{

    QVector<QSharedPointer<Node>> left, right;
    QVector<Token> compareTokens, conditionals;

    // Start
    Token t = m_currentToken;
    bool done=false;
    int linenum = m_currentToken.m_lineNumber;

    QSharedPointer<Node> clause = BinaryClause();

    int forcePage = findPage();

    if (m_currentToken.m_type==TokenType::THEN || m_currentToken.m_type==TokenType::DO)
        Eat(m_currentToken.m_type);
    else {
        ErrorHandler::e.Error("Expected THEN or DO after conditional", linenum);
    }

    QSharedPointer<Node> block = Block(false);

    QSharedPointer<Node> nodeElse = nullptr;
    if (m_currentToken.m_type==TokenType::ELSE) {
        Eat(TokenType::ELSE);
        nodeElse = Block(false);
    }

    return QSharedPointer<NodeConditional>(new NodeConditional(t, forcePage, clause, block, isWhileLoop, nodeElse));
}


QVector<QSharedPointer<Node>> Parser::StatementList()
{
    // Keep the current statement list for optimizations
    QVector<QSharedPointer<Node>> results;
    m_currentStatementList = &results;

    QSharedPointer<Node> node = Statement();
    if (node!=nullptr)
        results.append(node);

    while (m_currentToken.m_type == TokenType::SEMI) {
        Eat(TokenType::SEMI);
        QSharedPointer<Node> n = Statement();
        if (n!=nullptr)
           results.append(n);

    }
    if (m_currentToken.m_type==TokenType::ID)
//        ErrorHandler::e.Error("Parser::Statementlist SYNTAX ERROR : Token should not be ID", m_currentToken.m_lineNumber);
    ErrorHandler::e.Error("Did you forget a semicolon? (Token should not be ID in Parser)", m_currentToken.m_lineNumber);
    m_currentStatementList = nullptr;


    return results;

}

QSharedPointer<Node> Parser::CompoundStatement()
{
    if (m_currentToken.m_type!=TokenType::BEGIN) {
        // Single statement
        QSharedPointer<Node> n =  Statement();
  //      Eat(TokenType::SEMI);
    //    qDebug() << m_currentToken.getType();
      //  qDebug() << m_currentToken.m_value;

        return n;
    }
    Token t = m_currentToken;
    Eat(TokenType::BEGIN);
    QVector<QSharedPointer<Node>> nodes = StatementList();
    Eat(TokenType::END);
    QSharedPointer<NodeCompound> root = QSharedPointer<NodeCompound>(new NodeCompound(t));
    for (QSharedPointer<Node> n: nodes)
        root->children.append(n);

//    qDebug() << "from begin block : " +m_currentToken.getType();
    return root;

}

QSharedPointer<Node> Parser::Program(QString param)
{
//    QSharedPointer<Node> n = CompoundStatement();
    if (!m_isTRU)
        Eat(TokenType::PROGRAM);
    else
        Eat(TokenType::UNIT);

//    QSharedPointer<NodeVar> varNode = qSharedPointerDynamicCast<NodeVar>Variable();
    QString progName = m_currentToken.m_value;// varNode->value;
    // Prefix all TRUs with the name

    Eat();
    m_symTab->Define(QSharedPointer<Symbol>(new Symbol(progName,"STRING")));
    Eat(TokenType::SEMI);
    QSharedPointer<NodeBlock> block;

    if (m_isTRU) {
        m_symTab->m_gPrefix = progName+"_";
        m_symTab->m_currentUnit = m_symTab->m_gPrefix;
        if (s_usedTRUNames.contains(progName))
            ErrorHandler::e.Error("TRU '"+progName+"' is already defined! ",m_currentToken.m_lineNumber);
        s_usedTRUNames.append(progName);
    }
    else
        m_symTab->m_gPrefix = "";


    if (!m_isTRU)
        block = qSharedPointerDynamicCast<NodeBlock>(Block(true));
    else
        block = qSharedPointerDynamicCast<NodeBlock>(BlockNoCompound(true));



    QSharedPointer<NodeProgram> program = QSharedPointer<NodeProgram>(new NodeProgram(progName,  param, block));
//    if (block!=nullptr)
        ApplyTPUAfter(block->m_decl,m_proceduresOnly);

    if (!m_isTRU) {
        Eat(TokenType::DOT);
    }
    else {
        Eat(TokenType::END);
        Eat(TokenType::DOT);

    }
    emit EmitTick("&100"); // 100

    return program;

}


QSharedPointer<Node> Parser::Factor()
{
    if (m_currentToken.m_type == TokenType::LENGTH) {
        Eat();
        Eat(TokenType::LPAREN);
        QString varName = m_currentToken.m_value;
        QSharedPointer<Symbol> s = m_symTab->Lookup(varName,m_currentToken.m_lineNumber);
        if (s==nullptr) {
            ErrorHandler::e.Error("Internal function 'Length' reqruires a variable");
        }
        Token t = m_currentToken;
        t.m_intVal = s->getLength();
        t.m_type  = TokenType::INTEGER_CONST;
        Eat();
        Eat(TokenType::RPAREN);
        return QSharedPointer<NodeNumber>(new NodeNumber(t,s->getLength()));

    }

    if (m_currentToken.m_type == TokenType::SIZEOF) {
        Eat();
        Eat(TokenType::LPAREN);
        int len = 0;
        QString varName = m_currentToken.m_value;
        if (m_symTab->m_records.contains(varName))
        {
            len = m_symTab->m_records[varName]->getSize();
        }
        else {
            QSharedPointer<Symbol> s = m_symTab->Lookup(varName,m_currentToken.m_lineNumber);
            if (s==nullptr) {
                ErrorHandler::e.Error("Internal function 'Length' reqruires a variable");
            }
            len = s->getLength();

        }
        Token t = m_currentToken;
        t.m_intVal = len;
        t.m_type  = TokenType::INTEGER_CONST;
        Eat();
        Eat(TokenType::RPAREN);
        return QSharedPointer<NodeNumber>(new NodeNumber(t,len));

    }


    Token t = m_currentToken;

    if (t.m_type==TokenType::TEOF)
        ErrorHandler::e.Error("Syntax error", m_currentToken.m_lineNumber);


    if (t.m_type == TokenType::INTEGER_CONST || t.m_type ==TokenType::REAL_CONST
            || t.m_type ==TokenType::ADDRESS) {
        Eat(t.m_type);
  //      qDebug() << "parser: " <<t.m_value << t.m_intVal;
        return QSharedPointer<NodeNumber>(new NodeNumber(t, t.m_intVal));
    }

    if (t.m_type == TokenType::PLUS || t.m_type==TokenType::MINUS ){
        Eat(t.m_type);
        QSharedPointer<Node> expr = Factor();
        if (expr->isPureNumeric()) {
            bool isMinus = t.m_type==TokenType::MINUS;
            t.m_type = TokenType::INTEGER_CONST;
            t.m_value = "";
            t.m_intVal = expr->getValueAsInt(nullptr);
            if (isMinus) {
                if (t.m_intVal<256)
                    t.m_intVal = 256-t.m_intVal;
                else
                    t.m_intVal = 65536-t.m_intVal;
            }
            return QSharedPointer<NodeNumber>(new NodeNumber(t,t.m_intVal));


        }
        return QSharedPointer<NodeUnaryOp>(new NodeUnaryOp(t, expr));
    }




    if (t.m_type == TokenType::LPAREN) {
        Eat(TokenType::LPAREN);
        QSharedPointer<Node> node = Expr();
        Eat(TokenType::RPAREN);
        return node;

    }
    if (t.m_type == TokenType::ID) {
//        qDebug() << "FINDING PROCEDURE IN TERM: " << t.m_value;
        bool isAssign;
        QSharedPointer<Node> node = FindProcedure(isAssign, nullptr);
        if (node!=nullptr)
            return node;
        node = BuiltinFunction();
        if (node!=nullptr)
            return node;

    }
    return Variable();
}

QSharedPointer<Node> Parser::RepeatUntil()
{
    Token t = m_currentToken;
    Eat(TokenType::REPEAT);
    QVector<QSharedPointer<Node>> nodes = StatementList();
    QSharedPointer<NodeCompound> root = QSharedPointer<NodeCompound>(new NodeCompound(t));
    Eat(TokenType::UNTIL);
    QSharedPointer<Node> cond = BinaryClause();
    QVector<QSharedPointer<Node>> decl;

    for (QSharedPointer<Node> n: nodes)
        root->children.append(n);

    QSharedPointer<NodeBlock> block = QSharedPointer<NodeBlock>(new NodeBlock(t,decl,root));

//    qDebug() << "from begin block : " +m_currentToken.getType();
//    NodeRepeatUntil(Token op, int forcePage, QSharedPointer<Node> cond, QSharedPointer<Node> block);

    return QSharedPointer<Node>(new NodeRepeatUntil(t,false,qSharedPointerDynamicCast<NodeBinaryClause>(cond),block));

}

QSharedPointer<Node> Parser::Term()
{
    QSharedPointer<Node> node = Factor();
    while (m_currentToken.m_type == TokenType::Type::MUL || m_currentToken.m_type == TokenType::Type::DIV
    || m_currentToken.m_type == TokenType::Type::BITAND || m_currentToken.m_type == TokenType::Type::BITOR
     || m_currentToken.m_type == TokenType::Type::SHR || m_currentToken.m_type == TokenType::Type::SHL
           || m_currentToken.m_type == TokenType::Type::XOR

           ){
        Token t = m_currentToken;
        Eat(m_currentToken.m_type);

        node = QSharedPointer<NodeBinOP>(new NodeBinOP(node, t, Factor()));
    }
    return node;
}

void Parser::PreprocessSingle() {
//              qDebug() << "***PRE" << m_currentToken.m_value << m_pass;


  /*            if (m_currentToken.m_value.toLower()=="include") {

  //                QString str = m_currentToken.m_value;
                  Eat(TokenType::PREPROCESSOR);
                  QString name = m_currentToken.m_value;
                  QString filename =(m_currentDir +"/"+ m_currentToken.m_value);
                  filename = filename.replace("//","/");
                  QString text = m_lexer->loadTextFile(filename);
                  int ln=m_lexer->getLineNumber(m_currentToken.m_value)+m_acc;
                  m_lexer->m_text.insert(m_lexer->m_pos, text);
                  int count = text.split("\n").count();
                  m_lexer->m_includeFiles.append(
                              FilePart(name,ln, ln+ count, ln-m_acc,ln+count-m_acc,count));
                  m_acc-=count-1;

                  Eat(TokenType::STRING);
              }*/
              if (m_currentToken.m_value.toLower() =="deletefile") {
                  Eat(TokenType::PREPROCESSOR);
                  QString file = m_currentToken.m_value;
                  Eat();
                  if (QFile::exists(m_currentDir+file))
                      QFile::remove(m_currentDir+file);

              }
              else
              if (m_currentToken.m_value.toLower() =="define") {
                  Eat(TokenType::PREPROCESSOR);
                  QString key = m_currentToken.m_value;
                  Eat();
  //                qDebug() << m_currentToken.m_value;
      //            int i = getIntVal(m_currentToken);
    //              qDebug() << "After: " << Util::numToHex(i);
                  QString val = m_currentToken.m_value;
                  if (val=="")
                      val = QString::number(m_currentToken.m_intVal);

                  if (m_pass == PASS_PRE || m_pass == PASS_FIRST)
                      m_preprocessorDefines[key] = val;

//                  if (m_pass == PASS_PRE)
  //                    qDebug() << "Defined: " << key << val;

              }
              if (m_currentToken.m_value.toLower() == "splitfile") {
                  Eat(TokenType::PREPROCESSOR);
                  QString in = m_currentDir+QDir::separator()+ m_currentToken.m_value;
                  Eat(TokenType::STRING);
                  QString f1 = m_currentDir+QDir::separator()+m_currentToken.m_value;
                  Eat(TokenType::STRING);
                  QString f2 = m_currentDir+QDir::separator()+m_currentToken.m_value;
                  Eat(TokenType::STRING);

                  int split = m_currentToken.m_intVal;


                  Eat(TokenType::INTEGER_CONST);
                  QByteArray ba = Util::loadBinaryFile(in);
                  if (split>=ba.count())
                      ErrorHandler::e.Error("splitfile split position must be lower than file size!",m_currentToken.m_lineNumber);
                  QByteArray b1 = ba.mid(0,split);
                  QByteArray b2 = ba.mid(split,ba.count());
                  Util::SaveByteArray(b1, f1);
                  Util::SaveByteArray(b2, f2);
              }
              if (m_currentToken.m_value.toLower() =="addmonitorcommand") {
                  Eat(TokenType::PREPROCESSOR);
                  QString cmd = m_currentToken.m_value;
                  Eat();
                  m_symTab->m_extraMonCommands.append(cmd);

              }

              if (m_currentToken.m_value.toLower() =="setvalue") {
                  Eat(TokenType::PREPROCESSOR);
                  QString key = m_currentToken.m_value;
                  Eat();
  //                qDebug() << m_currentToken.m_value;
      //            int i = getIntVal(m_currentToken);
    //              qDebug() << "After: " << Util::numToHex(i);
                  int ival = 0;
                  bool isString = true;
                  QString val = m_currentToken.m_value;
                  if (val=="") {
                      isString = false;
                      ival = m_currentToken.m_intVal;
                  }
                  QSharedPointer<CIniFile> f = m_projectIni;

                  if (m_settingsIni->contains(key))
                      f = m_settingsIni;
                  if (isString)
                      f->setString(key,val);
                  else
                      f->setFloat(key,ival);

//                  if (m_pass == PASS_PRE)
  //                    qDebug() << "Defined: " << key << val;

              }
              else if (m_currentToken.m_value.toLower() =="ignoresystemheaders") {
                  Eat(TokenType::PREPROCESSOR);
                  Syntax::s.m_currentSystem->m_systemParams["ignoresystemheaders"]="1";
                  Data::data.demomode = true;
              }

              else if (m_currentToken.m_value.toLower() =="userdata") {
                  Eat(TokenType::PREPROCESSOR);
                  QString from = m_currentToken.getNumAsHexString();
                  Eat();
                  QString to = m_currentToken.getNumAsHexString();
                  Eat();
                  QString name = m_currentToken.m_value;
                  bool ok;
                  m_userBlocks.append(QSharedPointer<MemoryBlock>(new MemoryBlock(Util::NumberFromStringHex(from), Util::NumberFromStringHex(to),
                                                  MemoryBlock::USER, name)));

              }
              else if (m_currentToken.m_value.toLower() =="projectsettings") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleProjectSettingsPreprocessors();

              }
              else if (m_currentToken.m_value.toLower() =="buildpaw") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleBuildPaw();
              }
              else if (m_currentToken.m_value.toLower() =="spritepacker") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleSpritePacker();
              }
              else if (m_currentToken.m_value.toLower()=="compile_akg_music") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleAKGCompiler();
              }
              else if (m_currentToken.m_value.toLower()=="execute") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleExecute();
              }


              else if (m_currentToken.m_value.toLower() =="ignoremethod") {
                  Eat(TokenType::PREPROCESSOR);
                  m_ignoreMethods.append(m_currentToken.m_value);
              }
              else if (m_currentToken.m_value.toLower() =="export") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleExport();
              }
              else if (m_currentToken.m_value.toLower() =="compress") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleCompress();
              }
              else if (m_currentToken.m_value.toLower() =="export_parallax_data") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleExportParallaxData();
              }
              else if (m_currentToken.m_value.toLower()=="perlinnoise") {
                  Eat(TokenType::PREPROCESSOR);
                  HandlePerlinNoise();
              }
              else if (m_currentToken.m_value.toLower() =="setcompressionweights") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleSetCompressionWeights();
              }
              else if (m_currentToken.m_value.toLower() =="exportblackwhite") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleExportBW();
              }
              else if (m_currentToken.m_value.toLower() =="exportcompressed") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleExportCompressed();
              }
              else if (m_currentToken.m_value.toLower() =="macro") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleMacro();
              }
              else if (m_currentToken.m_value.toLower() =="exportrgb8palette") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleExportPalette();
              }
              else if (m_currentToken.m_value.toLower() =="exportprg2bin") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleExportPrg2Bin();
              }
              else if (m_currentToken.m_value.toLower()=="vicmemoryconfig") {
                  Eat(TokenType::PREPROCESSOR);
                  m_vicMemoryConfig = m_currentToken.m_value;
                  Eat();
              }
              else if (m_currentToken.m_value.toLower() =="pbmexport") {
                  Eat(TokenType::PREPROCESSOR);
                  HandlePBMExport();
              }
              else if (m_currentToken.m_value.toLower() =="vbmexport") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleVBMExport();
              }
              else if (m_currentToken.m_value.toLower() =="vbmexportcolor") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleVBMExportColor();
              }
              else if (m_currentToken.m_value.toLower() =="vbmexportchunk") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleVBMExportChunk();
              }
              else if (m_currentToken.m_value.toLower() =="exportframe") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleExportFrame();
              }
              else if (m_currentToken.m_value.toLower() =="spritecompiler") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleSpriteCompiler();
              }

              else if (m_currentToken.m_value.toLower() =="importchar") {
                  Eat(TokenType::PREPROCESSOR);
                  HandleImportChar();
              }


              else if (m_currentToken.m_value.toLower() =="startassembler") {
                  Eat(TokenType::PREPROCESSOR);
                  m_initAssembler = m_currentToken.m_value;
                  //m_ignoreMethods.append(m_currentToken.m_value);
              }
              else if (m_currentToken.m_value.toLower() =="requirefile") {
                  Eat();
                  QString requiredFile = m_currentToken.m_value;
                  Eat();
                  QString message = m_currentToken.m_value;
                  if (!QFile::exists(m_currentDir+"/"+requiredFile))
                      ErrorHandler::e.Error("The following file is required for compilation: <font color=\"#FF80A0\">'" + requiredFile + "'</font>.<br><font color=\"#FFB060\">" +message+"</font>", m_currentToken.m_lineNumber);

              }
              else if (m_currentToken.m_value.toLower() =="raisewarning") {
                  Eat();
              }
              else if (m_currentToken.m_value.toLower() =="raiseerror") {
                  Eat();
  /*                qDebug() << "PARSER RAISEERROR " <<m_ignoreAll <<m_pass;
                  if (!m_ignoreAll)
                      ErrorHandler::e.Error(m_currentToken.m_value, m_currentToken.m_lineNumber);
                      */
              }
              else if (m_currentToken.m_value.toLower() =="use") {
                  Eat();
                  QString type = m_currentToken.m_value;
                  bool ok=false;
                  if (type.toLower()=="krillsloader") {
                      ok=true;
                      int ln = Pmm::Data::d.lineNumber;

                      //m_lexer->m_lines.removeAt(ln);
                      //m_lexer->m_orgText.replace(orgL,"\n");
                      Eat();
                      int loaderPos = m_currentToken.m_intVal;
                      Eat();
                      int loaderOrgPos = m_currentToken.m_intVal;
                      Eat();
                      int installerPos = m_currentToken.m_intVal;


                      m_preprocessorDefines["_InstallKrill"] = Util::numToHex(installerPos + 0x1390);
                      m_preprocessorDefines["_LoadrawKrill"] = Util::numToHex(loaderPos);
  //                    m_preprocessorDefines["_LoadrawKrill"] = Util::numToHex(loaderPos);
  //                    qDebug() << m_preprocessorDefines["_LoadrawKrill"];
                      m_preprocessorDefines["_ResidentLoaderSource"] = Util::numToHex(loaderOrgPos);
                      m_preprocessorDefines["_ResidentLoaderDestination"] = Util::numToHex(loaderPos);


                      QString pos = QString::number(loaderPos,16);
                      if (pos=="200") pos = "0200";
                      QString loaderFile =":resources/bin/krill/loader_PAL_NTSC_"+pos.toUpper()+"-c64.prg";
                      QString installerFile =":resources/bin/krill/install_PAL_NTSC_"+QString::number(installerPos,16).toUpper()+"-c64.prg";

                      if (!QFile::exists(loaderFile))
                          ErrorHandler::e.Error("When using krills loader, the loader location must be either 0200, 1000,2000 etc");

                      if (!QFile::exists(installerFile))
                          ErrorHandler::e.Error("When using krills loader, the installer location must be either 1000, 2000, 3000 etc");


                      QString outFolder = m_currentDir+"/auto_bin/";
                      QString outFolderShort = "auto_bin/";

                      if (!QDir().exists(outFolder))
                              QDir().mkdir(outFolder);

                      QString outFile = outFolder+"krill_loader.bin";
                      if (QFile::exists(outFile)) {
                          QFile f(outFile);
                          f.remove();
                      }
  //                    QFile::copy(loaderFile, outFile);
                      Util::ConvertFileWithLoadAddress(loaderFile,outFile);

                      outFile = outFolder+"krill_installer.bin";
                      if (QFile::exists(outFile)) {
                          QFile f(outFile);
                          f.remove();
                      }
                      Util::ConvertFileWithLoadAddress(installerFile,outFile);
  //                    QFile in(installerFile);
    //                  QByteArray data =

                      //QFile::copy(installerFile, outFile);

                      outFile = outFolderShort+"krill_loader.bin";
                      QString replaceLine = "_ResidentLoader_Binary: 	incbin (\""+outFile+ "\",$"+QString::number(loaderOrgPos,16)+");";
                      outFile = outFolderShort+"krill_installer.bin";
                      replaceLine += "\n_Installer_Binary: 	incbin (\""+outFile+ "\",$"+QString::number(installerPos,16)+");";

                      for (QString s: m_diskFiles) {
                          QString var = s;
                          for (int i=0;i<256;i++) {
                              QString r = "#P"+QString::number(i)+";";
                              var = var.replace(r,"");
  //                            s = s.replace(r,QChar(i));
   //                           s = s.replace(r,"\""  +QString::number(i)  + "\"");
                          }

                          replaceLine+= var + ": string=(\""+s.toUpper()+"\");";

                      }
                      QString orgL =  m_lexer->m_lines[ln];



                      m_lexer->m_text.replace(orgL,replaceLine+"\n\t");
                      m_lexer->m_pos-=orgL.count();


                      //Eat();
                  }
                  else {
  //                    if (m_pass==0)
                          HandleUseTPU(type);
                  }
    /*              if (!ok) {
                      ErrorHandler::e.Error("Uknown @use parameter : "+type, m_currentToken.m_lineNumber);
                  }*/
              }
              else {
                  if (m_macros.contains(m_currentToken.m_value.toLower()))
                          HandleCallMacro(m_currentToken.m_value.toLower(),false);
                  else
                     Eat();
              }

            /*  else if (m_currentToken.m_value.toLower() =="error") {
                  Eat(TokenType::PREPROCESSOR);
                  ErrorHandler::e.Error("Error from preprocessor3: " +m_currentToken.m_value);

              }*/

}


void Parser::PreprocessAll()
{
    m_lexer->Initialize();
    m_lexer->m_ignorePreprocessor = false;
    m_acc = 0;
    m_currentToken = m_lexer->GetNextToken();
    //m_preprocessorDefines.clear();
    while (m_currentToken.m_type!=TokenType::TEOF) {
//        qDebug() << m_currentToken.getType() << m_currentToken.m_value;


        if (m_currentToken.m_type == TokenType::PREPROCESSOR) {
            PreprocessSingle();
        }
        else Eat();
//        if (m_currentToken.m_type!=TokenType::PREPROCESSOR)
  //          Eat();
        //qDebug() << "VAL: " << m_currentToken.m_value << m_currentToken.getType();

    }

    // Afterwards, replace all preprocessor defines
//    PreprocessIfDefs();
    PreprocessReplace();
}

void Parser::PreprocessReplace()
{

    for (QString k: m_preprocessorDefines.keys()) {
        QString val = m_preprocessorDefines[k];
//        qDebug() << "Replacing: @" + k << "  with " << val;
        QRegularExpression rg = QRegularExpression("@\\b"+k+"\\b");
        //qDebug() << rg;

//        m_lexer->m_text = m_lexer->m_text.replace("@" +k, val);
        m_lexer->m_text = m_lexer->m_text.replace(rg, val);

    }

  //  qDebug() << m_lexer->m_text;
//    exit(1);

//    qDebug() << m_preprocessorDefines.keys();
}

QSharedPointer<Node> Parser::Parse(bool removeUnusedDecls, QString param, QString globalDefines, bool useLocals)
{
    // Call preprocessor for include files etc
    m_lexer->m_orgText = m_lexer->m_orgText + "\n" + globalDefines+"\n";
    m_lexer->m_text = m_lexer->m_orgText;
    m_removeUnusedDecls = removeUnusedDecls;
    Node::m_curMemoryBlock = nullptr; //
    Node::m_staticBlockInfo.m_blockID = -1;
    LabelStack::m_labelCount = 0;
    if (!m_isTRU)
        Data::data.demomode = false;

    m_vicMemoryConfig = m_projectIni->getString("vic_memory_config");

    if (!m_isTRU || m_symTab==nullptr)
        m_symTab = QSharedPointer<SymbolTable>(new SymbolTable());

    // Reset the node count
    if (!m_isTRU)
        Node::s_nodeCount = 0;


    m_symTab->m_currentFilename = m_currentFileShort;
    emit EmitTick("<font color=\"grey\">Parsing: []");
    m_pass = 0;
  //  RemoveComments();
    InitObsolete();

    Symbol::s_currentProcedure = "main";
    m_inCurrentProcedure = "main";

    if (Syntax::s.m_currentSystem->m_processor!=AbstractSystem::M68000)
        StripWhiteSpaceBeforeParenthesis(); // TODO: make better fix for this

    Data::data.compilerState = Data::PREPROCESSOR;
    InitSystemPreprocessors();
    bool done = false;
    //while (!done)
    m_pass = PASS_FIRST;
//    m_symTab->m_constants.clear();
    PreprocessAll();
    m_pass = PASS_PRE;
    done = PreprocessIncludeFiles();
    PreprocessAll();
    ApplyTPUBefore();
//    qDebug().noquote() << "SOURCE" << m_lexer->m_text;
//    PreprocessConstants();
  //  m_pass = PASS_PRE;

//    PreprocessAll();

    m_pass = PASS_CODE;
    Data::data.compilerState = Data::PARSER;

//    if (!m_isTRU)
  //      m_tpus.clear();

    m_lexer->m_currentComment = "";
    m_parserBlocks.clear();
    m_symTab->m_useLocals = useLocals;


    Node::parserSymTab = m_symTab; // Clear all node flags
    Node::flags.clear();

    m_lexer->Initialize();
//    qDebug() << "B1 "<<m_symTab->m_symbols.keys();
    m_lexer->m_ignorePreprocessor = true;
    m_currentToken = m_lexer->GetNextToken();
    m_symTab->Initialize();
    InitSystemSymbols();

    Node::m_staticBlockInfo.m_blockID=-1;
    ErrorHandler::e.m_lexer = m_lexer.get(); // for Warnings


    /* MAIN PARSER
  _____
 |  __ \
 | |__) |_ _ _ __ ___  ___ _ __
 |  ___/ _` | '__/ __|/ _ \ '__|
 | |  | (_| | |  \__ \  __/ |
 |_|   \__,_|_|  |___/\___|_|

                                */
    QSharedPointer<NodeProgram> root = qSharedPointerDynamicCast<NodeProgram>(Program(param));


    // First add builtin functions
    if (removeUnusedDecls && !m_isTRU) {
        RemoveUnusedProcedures();
        RemoveUnusedSymbols(root);
    }


    InitBuiltinFunctions();


    for (QString s: m_procedures.keys())
        if (qSharedPointerDynamicCast<NodeProcedureDecl>(m_procedures[s])->m_block==nullptr) {
            root->m_NodeBlock->m_decl.append(m_procedures[s]);
        }


    // Then add regular ones ORDERED BY DEFINITION
    //for (QString s: m_procedures.keys())
     //   if (((QSharedPointer<NodeProcedureDecl>)m_procedures[s])->m_block!=nullptr)
    for ( QSharedPointer<Node>& n: m_proceduresOnly )
        root->m_NodeBlock->m_decl.append(n);


    if (m_currentToken.m_type!=TokenType::TEOF)
        ErrorHandler::e.Error("End of file error");


    if (Node::m_staticBlockInfo.m_blockID !=-1) {
//        ErrorHandler::e.Error("Cannot end program with open blocks. Please use the corresponding @endblock command to close the open block at "+Node::m_staticBlockInfo.m_blockPos,0);
    }


//    qDebug() << m_symTab->m_symbols.keys();
    m_projectIni->setString("temp_vic_fapmemory_config",m_vicMemoryConfig);
    return root;
}


QSharedPointer<Node> Parser::FindProcedure(bool& isAssign,QSharedPointer<Node> parentExpr = nullptr)
{
    isAssign = false;

    Token procToken = m_currentToken;
    bool dual = false;
    // TRUs can also search for other TRU names without prefix
    if (m_isTRU)
        dual = m_procedures.contains(m_currentToken.m_value);

//    qDebug() << "Searching for procedure : " <<m_currentToken.m_value;// << m_procedures.keys();
  //  qDebug() << "Searching for procedure : " <<m_currentToken.m_value;
    // Already defined? calling / assigning an existing procedure?

    if (m_procedures.contains(m_symTab->m_gPrefix+m_currentToken.m_value) || dual) {
    //    qDebug() << "FOUND";
        QString procName = m_symTab->m_gPrefix+m_currentToken.m_value; // fix prefix
        if (dual) procName = m_currentToken.m_value;;
        Token t = m_currentToken;
        Eat(TokenType::ID);

        // Return value from "Function"
        if (m_currentToken.m_type==TokenType::ASSIGN) { // IS function ASSIGN myProc:= ... - so return value
            // ASSIGN procedure for return value
//            qDebug() << procName << m_inCurrentProcedure;
            if (procName != m_inCurrentProcedure)
                ErrorHandler::e.Error("Function return name needs to be identical the name of the function itself.",m_currentToken.m_lineNumber);
            Eat();
            auto node = qSharedPointerDynamicCast<NodeProcedureDecl>(m_procedures[procName]);
            if (!node->m_isFunction)
                ErrorHandler::e.Error("Only functions can have intrinsic return values.",m_currentToken.m_lineNumber);
            if (node->m_returnValue!=nullptr)
                ErrorHandler::e.Error("You can only set the return value once in the scope of the function.",m_currentToken.m_lineNumber);

            node->m_returnValue = Expr();
            isAssign=true;
            return nullptr;
        }
        QVector<QSharedPointer<Node>> paramList;
        QSharedPointer<NodeProcedureDecl> p = qSharedPointerDynamicCast<NodeProcedureDecl>(m_procedures[procName]);
        if (m_currentToken.m_type!=TokenType::SEMI && m_currentToken.m_type!=TokenType::RPAREN)  {
            Eat(TokenType::LPAREN); // Must be a procedure call
            while (m_currentToken.m_type!=TokenType::RPAREN && !m_lexer->m_finished) {
                paramList.append(Expr());

                if (m_currentToken.m_type==TokenType::COMMA)
                    Eat(TokenType::COMMA);
                //if (m_currentToken.m_type==TokenType::SEMI)
                //    ErrorHandler::e.Error("Syntax errror", m_currentToken.m_lineNumber);
            }
            //        qDebug() << "Searching for :" << procName << " in " << m_procedures.keys();
            if (!m_procedures.contains(procName))
                ErrorHandler::e.Error("Could not find procedure :" + procName, m_currentToken.m_lineNumber);

            Eat(TokenType::RPAREN);
        }
        //p->SetParameters(paramList);
        p->m_isUsed = true;
        if (m_inCurrentProcedure != "")
            p->m_isUsedBy<<m_inCurrentProcedure;
        //        if (p->m_procName==BGMUpdateSpriteLoc)

        if (m_addInitialReferenceToProcedureCall!="") {
            // Let's insert a parent reference automatically! for classes.
            auto classRef = m_currentToken;
            classRef.m_value = m_addInitialReferenceToProcedureCall;

            QSharedPointer<Symbol> s = m_symTab->Lookup(classRef.m_value,m_currentToken.m_lineNumber);
             if (s->m_type.toLower()!="pointer")
                classRef.m_isReference = true;
            // Inserts extra FIRST parameter
             auto nv = QSharedPointer<NodeVar>(new NodeVar(classRef));
/*             nv->m_expr = parentExpr;
//             qDebug() << nv->m_expr->getValue(nullptr);
            paramList.insert(0,ApplyClassVariable(nv));*/
        }

        return QSharedPointer<NodeProcedure>(new NodeProcedure(p, paramList, t));
    }

    //qDebug() << m_currentToken.getType() << " with value " << m_currentToken.m_value;
    return nullptr;
}


QSharedPointer<Node> Parser::Block(bool useOwnSymTab, QString blockName)
{

/*    if (m_currentToken.m_type!=TokenType::VAR  && m_currentToken.m_type!=TokenType::BEGIN)
        return nullptr;
*/
    // Main block
//    qDebug() <<blockName <<m_inCurrentProcedure;
    if (m_inCurrentProcedure!="main")
    if (m_currentToken.m_type==TokenType::PROCEDURE || m_currentToken.m_type==TokenType::INTERRUPT || m_currentToken.m_type==TokenType::WEDGE || m_currentToken.m_type==TokenType::FUNCTION)
        return nullptr;

    QVector<QSharedPointer<Node>> decl =  Declarations(useOwnSymTab, blockName);

    int pos = m_currentToken.m_lineNumber;
    QSharedPointer<Node> statements =  CompoundStatement();
    QSharedPointer<NodeBlock> bl =  QSharedPointer<NodeBlock>(new NodeBlock(m_currentToken,decl, statements, useOwnSymTab));
    bl->m_op.m_lineNumber = pos;
    return bl;
}

QSharedPointer<Node> Parser::BlockNoCompound(bool useOwnSymTab, QString blockName)
{

//    if (m_currentToken.m_type==TokenType::PROCEDURE || m_currentToken.m_type==TokenType::INTERRUPT || m_currentToken.m_type==TokenType::WEDGE || m_currentToken.m_type==TokenType::FUNCTION)
  //      return nullptr;

    if (m_currentToken.m_type!=TokenType::VAR)
        ErrorHandler::e.Error("TRUs must contain at least one \"var\" declaration block.", m_currentToken.m_lineNumber);



    QVector<QSharedPointer<Node>> decl =  Declarations(useOwnSymTab, blockName, false);
    int pos = m_currentToken.m_lineNumber;
    QSharedPointer<NodeBlock> bl =  QSharedPointer<NodeBlock>(new NodeBlock(m_currentToken,decl, nullptr, useOwnSymTab));
    bl->m_op.m_lineNumber = pos;
    return bl;

}


QVector<QSharedPointer<Node> > Parser::Parameters(QString blockName)
{
    QVector<QSharedPointer<Node>> decl;
    if (m_currentToken.m_type==TokenType::LPAREN) {
        Eat(TokenType::LPAREN);
        while (m_currentToken.m_type==TokenType::ID) {
            QVector<QSharedPointer<Node>> ns = VariableDeclarations(blockName,true);

            for (QSharedPointer<Node> n: ns) {
                decl.append(n);
            }
            Eat(m_currentToken.m_type);
        }
    }
    //Eat(TokenType::RPAREN);
    return decl;
}

QSharedPointer<Node> Parser::ForLoop(bool inclusive)
{
    int ln = m_currentToken.m_lineNumber;
    if (inclusive)
        Eat(TokenType::FORI);
    else
        Eat(TokenType::FOR);
    QSharedPointer<Node> a = AssignStatement();
    Eat(TokenType::TO);
    QSharedPointer<Node> b = Expr();
    bool unroll = false;
    QSharedPointer<Node> step = nullptr;

    int forcePage = 0;
    int loopType = 0; // use var
    int curCnt=0;
    while (m_currentToken.m_type!=TokenType::DO) {
        if (m_currentToken.m_type==TokenType::ONPAGE || m_currentToken.m_type==TokenType::OFFPAGE)
            forcePage = findPage();
        if (m_currentToken.m_type==TokenType::STEP) {
            Eat();
            step = Expr();
            //qDebug() << TokenType::getType(step->m_op.m_type);
        }
        if (m_currentToken.m_type==TokenType::LOOPX) {
            Eat();
            loopType = 1;
        }
        if (m_currentToken.m_type==TokenType::LOOPY) {
            Eat();
            loopType = 2;
        }
        if (m_currentToken.m_type==TokenType::UNROLL) {
            Eat();
            unroll = true;
        }
        if (curCnt++>15) {
            ErrorHandler::e.Error("For loop needs a 'DO' keyword", ln);
        }
    }


    Eat(m_currentToken.m_type);
//    qDebug() << "Current: " << m_currentToken.getType();
//    Eat(TokenType::DO);
    QSharedPointer<Node> block = Block(false);

//    qDebug() << m_currentToken.getType();
  //  exit(1);
    return QSharedPointer<NodeForLoop>(new NodeForLoop(a,b,block, step, unroll, forcePage, loopType, inclusive));

}

QSharedPointer<Node> Parser::String(bool isCString = false)
{

    if (m_currentToken.m_type==TokenType::STRING || m_currentToken.m_type==TokenType::CSTRING) {
        QSharedPointer<NodeString> node = QSharedPointer<NodeString>(new NodeString(m_currentToken,QStringList()<<m_currentToken.m_value,m_currentToken.m_type==TokenType::CSTRING));
        Eat();
        return node;
    }
    if (m_currentToken.m_type!=TokenType::LPAREN)
        ErrorHandler::e.Error("String declaration must be single string or paranthesis with multi values.", m_currentToken.m_lineNumber);


    Eat();

    Token token(TokenType::STRING, m_currentToken.m_value);
    //m_currentToken.m_type = TokenType::STRING;
    QStringList lst;
//    lst<<m_currentToken.m_value;
    int max=0;
    QString numID = "";
    if (isCString)
        numID = "*&NUM";
    while (m_currentToken.m_type!=TokenType::RPAREN) {
        //GetParsedInt(TokenType::INTEGER);

        if (m_currentToken.m_value=="" || m_currentToken.m_type==TokenType::ID || m_currentToken.m_type==TokenType::LPAREN)
            m_currentToken.m_value = numID + QString::number(GetParsedInt(TokenType::INTEGER));

/*        if (m_currentToken.m_value=="")
            m_currentToken.m_value = QString::number();
*/

        if (m_currentToken.m_value!="")
            lst<<m_currentToken.m_value;

        if (m_currentToken.m_type == TokenType::RPAREN)
            break;
        Eat();

        if (m_currentToken.m_type==TokenType::COMMA)
            Eat();
        if (max++>10000)
            ErrorHandler::e.Error("String error!", token.m_lineNumber);
    }
    Eat(); // RParen
//    qDebug() <<m_currentToken.getType();
    QSharedPointer<NodeString> node = QSharedPointer<NodeString>(new NodeString(token, lst, token.m_type==TokenType::CSTRING));
    return node;
}


void Parser::VarDeclarations(QVector<QSharedPointer<Node>>& decl, QString blockName) {
    if (m_currentToken.m_type == TokenType::VAR)
        Eat(TokenType::VAR);
    while (m_currentToken.m_type==TokenType::ID || m_currentToken.m_type == TokenType::CONSTANT || m_currentToken.m_type == TokenType::TYPE) {
        if (m_currentToken.m_type == TokenType::CONSTANT) {
            ConstDeclaration();
        } else
        if (m_currentToken.m_type == TokenType::TYPE) {
            TypeDeclaration();
        }
        else {

            QVector<QSharedPointer<Node>> ns = VariableDeclarations(blockName);

            for (QSharedPointer<Node> n: ns)
                decl.append(n);
        }
        Eat(TokenType::SEMI);
    }
}
void Parser::ProcDeclarations(QVector<QSharedPointer<Node>>& decl, QString blockName)
{
    int type=0;
    if (m_currentToken.m_type == TokenType::INTERRUPT) type=1;
    if (m_currentToken.m_type == TokenType::WEDGE) type=2;
    bool isFunction = m_currentToken.m_type == TokenType::FUNCTION;
 //   bool isInterrupt= (m_currentToken.m_type==TokenType::PROCEDURE)?false:true;
    Token tok = m_currentToken;
    Eat(m_currentToken.m_type);
    bool isInline = false;
    QString procName =m_procPrefix+ m_symTab->m_gPrefix+ m_currentToken.m_value;

//    qDebug() <<"Declaring procedure;: "<<procName;

    m_inCurrentProcedure = procName;
    Symbol::s_currentProcedure = procName;
    //qDebug() << tok.m_value  << " : " << procName;
    Eat(TokenType::ID);
    //exit(1);
    QVector<QSharedPointer<Node>> paramDecl;
    if (m_currentToken.m_type==TokenType::LPAREN)
        paramDecl = Parameters(procName);
    //qDebug()<< "current : " << m_currentToken.getType();
    // If no parameters
    if (m_currentToken.m_type==TokenType::RPAREN)
        Eat(TokenType::RPAREN);

    if (m_currentToken.m_type == TokenType::INLINE) {
        isInline = true;
        Eat(TokenType::INLINE);
    }
    QSharedPointer<Node> funcType;
    if (isFunction) {
        Eat(TokenType::COLON);
        funcType = TypeSpec(false,QStringList());
        auto t = qSharedPointerDynamicCast<NodeVarType>(funcType);
        QStringList allowed = QStringList() << "integer" <<"byte" <<"long";
        if (!(allowed.contains(t->value.toLower()))) {
            ErrorHandler::e.Error("TRSE currently only supports return values of type 'byte', 'integer' and 'long'",t->m_op.m_lineNumber);
        }


    }


    Eat(TokenType::SEMI);
    QSharedPointer<Node> block = nullptr;
    QSharedPointer<NodeProcedureDecl> procDecl = QSharedPointer<NodeProcedureDecl>(new NodeProcedureDecl(tok, procName, paramDecl, block, type));
//    qDebug() << "Starting new procedure decl block with : "<< procName << procDecl->m_blockInfo.m_blockID;
    procDecl->m_fileName = m_currentFileShort;
    procDecl->m_isInline = isInline;
    procDecl->m_isFunction = isFunction;
    AppendComment(procDecl);

    if (m_procedures[procName]!=nullptr) {
        procDecl->m_isUsed = m_procedures[procName]->m_isUsed;
        procDecl->m_isUsedBy = m_procedures[procName]->m_isUsedBy;
        // Make sure that the correct number of parameters + types etc are identical for the forward declared procedure
        QSharedPointer<NodeProcedureDecl> existing = qSharedPointerDynamicCast<NodeProcedureDecl>(m_procedures[procName]);
        if (existing->m_paramDecl.count()!=procDecl->m_paramDecl.count())
            ErrorHandler::e.Error("Forward declared procedure '"+ WashVariableName(procName) +"' has incorrect number of parameters. ", tok.m_lineNumber);

        for (int i=0;i<existing->m_paramDecl.count();i++) {
            QSharedPointer<NodeVarDecl> a = qSharedPointerDynamicCast<NodeVarDecl>(existing->m_paramDecl[i]);
            QSharedPointer<NodeVarDecl> b = qSharedPointerDynamicCast<NodeVarDecl>(procDecl->m_paramDecl[i]);
            if ( qSharedPointerDynamicCast<NodeVar>(a->m_varNode)->value!=
                 qSharedPointerDynamicCast<NodeVar>(b->m_varNode)->value)
                ErrorHandler::e.Error("Forward declared procedure '"+ WashVariableName(procName) +"' has incorrect or missing declared parameter '"+WashVariableName(qSharedPointerDynamicCast<NodeVar>(a->m_varNode)->value)+"'", tok.m_lineNumber);

            if ( qSharedPointerDynamicCast<NodeVarType>(a->m_typeNode)->value!=
                 qSharedPointerDynamicCast<NodeVarType>(b->m_typeNode)->value)
                ErrorHandler::e.Error("Forward declared procedure '"+ WashVariableName(procName) +
                                      "' has incorrect declared parameter type for parameter '"+WashVariableName(qSharedPointerDynamicCast<NodeVar>(a->m_varNode)->value)+"', should be "
                                      +WashVariableName(qSharedPointerDynamicCast<NodeVarType>(a->m_typeNode)->value), tok.m_lineNumber);


        }
//            qDebug() << procName << procDecl->m_paramDecl.count() << existing->m_paramDecl.count();

    }
    m_procedures[procName] = procDecl;
//        qDebug() << "Adding to GLOBAL list: " <<procName;
    m_symTab->m_globalList.append(procName);
    if (isFunction)
        qSharedPointerDynamicCast<NodeProcedureDecl>(m_procedures[procName])->m_returnType = funcType;



    // For recursive procedures, it is vital that we forward declare the current procedure
    block = Block(false, procName);
//        if (block==nullptr)
//          qDebug() << "Procedure decl: " << procName;
    //decl.append(procDecl);
    if (block!=nullptr)
        Eat(TokenType::SEMI);
    else {
        // Forward declared variables are used
        procDecl->m_isUsed = true;

    }

    procDecl->AppendBlock(block);
    //qDebug() <<procName;

    if (block!=nullptr) {
        bool ok = true;
         // Check if procedure already declared
        for (QSharedPointer<Node> n: m_proceduresOnly) {
            QSharedPointer<NodeProcedureDecl> proc =qSharedPointerDynamicCast<NodeProcedureDecl>(n);
            if (proc->m_procName==procName) {
                ok = false;
                // Verify that the parameters are identical:
            }


        }
        if (!ok)
            ErrorHandler::e.Error("Procedure '"+ procName +"' already defined", tok.m_lineNumber);

        m_proceduresOnly.append(procDecl);
    }


    Symbol::s_currentProcedure = "main";
    m_inCurrentProcedure = "main";
    decl.append(procDecl);
}

QSharedPointer<NodeNumber> Parser::CreateNumber(int i)
{
    Token t = m_currentToken;
    t.m_type=TokenType::INTEGER_CONST;
    t.m_intVal = i;
    return QSharedPointer<NodeNumber>(new NodeNumber(t,i));
}

QSharedPointer<NodeVar> Parser::CreateVariable(QString v)
{
    Token t = m_currentToken;
    t.m_type=TokenType::ID;
    t.m_value = v;
    t.m_intVal = 0;

    return QSharedPointer<NodeVar>(new NodeVar(t));

}

QSharedPointer<NodeBinOP> Parser::CreateBinop(TokenType::Type tt, QSharedPointer<Node> left, QSharedPointer<Node> right)
{
    Token t = m_currentToken;
    t.m_type = tt;
    return QSharedPointer<NodeBinOP>(new NodeBinOP(left,t,right));
}

QSharedPointer<NodeAssign> Parser::CreateAssign(QSharedPointer<Node> left, QSharedPointer<Node> right)
{
    Token t = m_currentToken;
    t.m_type = TokenType::ASSIGN;
    return QSharedPointer<NodeAssign>(new NodeAssign(left,t,right));

}
/*
 *  This recursive method transforms class variables into "flat" structures
 *  ie
 *  pm := #objects[i] ->  pm := #objects + i*sizeOf(Object)
 *  pm.y := 10  -> pm[3] := 10;
 *
 *  Note: it is the very *heart* of OOP in TRSE!
 *
 */
QSharedPointer<Node> Parser::ApplyClassVariable(QSharedPointer<Node> var)
{
    auto v = qSharedPointerDynamicCast<NodeVar>(var);
   if (!v)
        return var;

    if (v->m_classApplied)
        return v;

    QSharedPointer<Symbol> s = m_symTab->Lookup(v->value,m_currentToken.m_lineNumber);
    QString t1;

    auto sv = qSharedPointerDynamicCast<NodeVar>(v->m_subNode);
    auto type = s->getEndType();

//    qDebug() << "START WITH "<<v->value;
    if (!(m_symTab->m_records.contains(type) &&m_symTab->m_records[type]->m_isClass))
        return v;



    bool isArray = s->m_type.toLower()=="array";

    // objects[i].pos[2].y
    // objects[ sizeof(Object)*i].pos[2].y

//    v->m_expr = nullptr;
    int size = m_symTab->m_records[type]->getSize();
    // var + i*sizeOf(class)

    // First add shift
    int shiftInternal = 0;


//    qDebug() << "Shift Internal "<< shiftInternal <<"Has expr : " <<(v->m_expr!=nullptr);

    // After shift, we look up the NEW type and size

/*    QSharedPointer<Symbol> s2 = m_symTab->Lookup(v->value,m_currentToken.m_lineNumber);
    QString newType = s2->getEndType();
    if (m_symTab->m_records.contains(newType) && newType!=type)
    {
        size = m_symTab->m_records[newType]->getSize();
        qDebug() << "** UH OH, subnode is object of size" <<size;
        type = newType;
    }
    QString washed = v->value;
    washed = washed.remove(type+"_");*/
    if (size!=0 && v->m_expr!=nullptr) {
        // objects[i].a. -> objects[i*sizeof(Object)].a
        //qDebug() << "NEED A MUL BABY "<<size <<v->m_expr->numValue();
        auto nodeClassSize = CreateNumber(size);
        if (v->m_expr->isPureNumeric() && v->m_expr->numValue()==0) {

        }
        else
            v->m_expr = CreateBinop(TokenType::MUL,v->m_expr,nodeClassSize);
    }



    if (sv!=nullptr) {
//        qDebug() << "In subnode: Calculating SHIFT "<<type<<sv->value;
         shiftInternal = m_symTab->m_records[type]->getShiftedPositionOfVariable(sv->value,1);

        if (shiftInternal!=0) {
//            qDebug() << "internal shift for var "+sv->value+" : Adding "<<shiftInternal;
            if (v->m_expr!=nullptr)
                v->m_expr = CreateBinop(TokenType::PLUS,v->m_expr,CreateNumber(shiftInternal));
            else
                v->m_expr = CreateNumber(shiftInternal);
        }


  //      qDebug() <<  "***** SUBNODE PUSH "<<v->value;
        auto node = ApplyClassVariable(sv);
    //    qDebug() <<  "***** SUBNODE POP"<<v->value;
        auto sv2 = qSharedPointerDynamicCast<NodeVar>(node);
        node = sv2->m_expr;
        if (node!=nullptr) {
            if (v->m_expr!=nullptr)
                v->m_expr = CreateBinop(TokenType::PLUS,v->m_expr,node);
            else
                v->m_expr = node;
        }
        if (v->m_expr==nullptr)
            v->m_expr = CreateNumber(0);


        v->m_subNode = nullptr; // REMOVE subnode

    }




    v->m_classApplied = true;
    if (v->m_expr == nullptr)
        return v;
    //qDebug() << "FINAL "<<v->value  << v->m_expr->numValue();
    if (!v->isReference())
        return v;
    // REFERENCE! )

    //qDebug() << "IS REFERECE";
    auto add = v->m_expr;
    v->m_expr = nullptr;

    return CreateBinop(TokenType::PLUS, v,add);

}
/*
QSharedPointer<Node> Parser::ApplyClassVariable(QSharedPointer<Node> var)
{
    auto v = qSharedPointerDynamicCast<NodeVar>(var);
    if (!v)
        return var;



    QSharedPointer<Symbol> s = m_symTab->Lookup(v->value,m_currentToken.m_lineNumber);
    QString t1;


    QString type = s->getEndType();
    auto sv = qSharedPointerDynamicCast<NodeVar>(v->m_subNode);
    auto proc = qSharedPointerDynamicCast<NodeProcedure>(v->m_subNode);

    if (proc!=nullptr)  {
//        qDebug() <<"*** IS PROC WRONG" <<proc->m_procedure->m_procName;
    }

    if (var->isRecord(m_symTab,t1) && sv==nullptr)
        v->isPureObject = true;

    if (sv!=nullptr && sv->isRecord(m_symTab,t1)) {
        sv->isPureObject = true;
        v->isPureObject = true;
//        qDebug() << "PURE ";
  //      qDebug() << "SVAR " <<sv->value <<sv->isRecord(m_symTab,t1) <<t1;
    }

    if (!(m_symTab->m_records.contains(type) &&m_symTab->m_records[type]->m_isClass))
        return v;

    bool isArray = s->m_type.toLower()=="array";
//    qDebug() << "IS ARRAY: "<<v->value<<isArray;
    if (sv!=nullptr) { // class.property translates to class[pos_in_memory];
    {
        QString subVar = sv->value;
        // Ok we have a subvar: pm.x
        bool subvarIsArray = m_symTab->m_records[type]->m_symbols[subVar]->m_type.toLower()=="array";
//        qDebug() << "VAR "<<v->value<<subVar<< m_symTab->m_records[type]->m_symbols[subVar]->m_type <<subvarIsArray;
        int shiftInternal = m_symTab->m_records[type]->getShiftedPositionOfVariable(subVar,1);
        int dataLength = m_symTab->m_records[type]->m_symbols[subVar]->getCountingLength();
//        if (subvarIsArray) dataLength=1;
        if (m_symTab->m_records[type]->m_isClass)
            dataLength = 1;

        if (isArray && v->m_expr == nullptr)
            ErrorHandler::e.Error("'"+v->value+"' is an array, did you forget the index? ('"+v->value+"[i]' etc)",var->m_op.m_lineNumber);

        if (v->m_expr!=nullptr) {
            if (v->m_expr->isPureNumeric()) {
                shiftInternal+=v->m_expr->getValueAsInt(nullptr)*m_symTab->m_records[type]->getSize();
            }
            else {
                // For now, disallow looking up an object in a array with generic index
*/
                /* Here, magic happes:
                 * replace
                 *
                 * monster[i].x:=10
                 *
                 * with
                 *
                 * tempPointer1:=#monster[i];
                 * tempPonter1.x := 10;
                 *
                 * */
/*                QString tmp1 = m_symTab->m_tempPointers.pop();
                if (!m_symTab->m_symbols.contains(tmp1)) {
                    auto sym = QSharedPointer<Symbol>(new Symbol(tmp1,"POINTER"));
                    m_symTab->Define(sym);
                    sym->m_pointsTo = type;

                }

                auto ptr = CreateVariable(tmp1);
                auto refVal = CreateVariable(v->value);
                refVal->m_expr = v->m_expr;
                // but no subvar
                refVal->m_op.m_isReference = true;
                m_currentStatementList->append(CreateAssign(ptr,refVal));
                m_symTab->m_tempPointers.push(tmp1);
                return CreateVariable(tmp1);
*/
                /*
                // Force the user to handle the pointers instead. Don't worry about it!
                ErrorHandler::e.Error("Class array indices not supported yet. Please use pointers.",m_currentToken.m_lineNumber);
            }
        }


        v->m_expr = CreateNumber(shiftInternal);


        if (sv->m_expr!=nullptr) {
            // Uh oh! We are looking up something
            // like pm.data[ index ] - make a binop of *shift* and the index.
            // pm.data[index] := pm[shift + index];
            // Check if actually an array:
            //qDebug() <<"END TYPE " <<type << subVar<<m_symTab->m_records[type]->m_symbols[subVar]->m_type;
            if (subvarIsArray) dataLength=1;
            int dataLength = m_symTab->m_records[type]->m_symbols[subVar]->getCountingLength();
            qDebug() << "HERE" <<dataLength<<shiftInternal <<v->value<<subVar<<(v->m_expr==nullptr)<<(v->m_subNode==nullptr)<<subvarIsArray;

            if (dataLength!=1) // uh oh we need to mul with datalength
                sv->m_expr = CreateBinop(TokenType::MUL,sv->m_expr,CreateNumber(dataLength));
            v->m_expr = CreateBinop(TokenType::PLUS, sv->m_expr,v->m_expr);
        }
        v->m_subNode = nullptr;

        if (v->isReference()) {
            auto rhs = v->m_expr;
            v->m_expr = nullptr;
            v->m_op.m_isReference = false;
            return CreateBinop(TokenType::PLUS,v,rhs);
        }
//        qDebug() << "Parser applying class to "<<v->value <<v->m_expr->getValueAsInt(nullptr);

//        return QSharedPointer<NodeBinOP>(new NodeBinOP(var,t,num));
    }
    }
    else {
        // Check for #arr[i], convert to #arr +i*sizeof(class)
        if (v->m_expr == nullptr)
            return v;
        if (!v->isReference())
            return v;
        auto nodeCounter = v->m_expr;
        v->m_expr = nullptr;
        int size = m_symTab->m_records[type]->getSize();
        // var + i*sizeOf(class)
        auto nodeClassSize = CreateNumber(size);
        auto bopShift = CreateBinop(TokenType::MUL,nodeCounter,nodeClassSize);
        return CreateBinop(TokenType::PLUS, var,bopShift);

    }
    return v;
}

*/

QVector<QSharedPointer<Node>> Parser::Declarations(bool isMain, QString blockName, bool hasBlock )
{
    QVector<QSharedPointer<Node>> decl;

//    qDebug() << "PARSER " <<isMain <<m_currentToken.getType() << blockName;
  /*  if (isMain)
        if (m_currentToken.m_type!=TokenType::VAR)
            ErrorHandler::e.Error("Main program must start with 'var' keyword",m_currentToken.m_lineNumber);

*/

/*
    while (m_currentToken.m_type!=TokenType::BEGIN) {

        if ((m_currentToken.m_type==TokenType::PROCEDURE
                || m_currentToken.m_type==TokenType::FUNCTION
                || m_currentToken.m_type==TokenType::WEDGE
                || m_currentToken.m_type==TokenType::INTERRUPT
                ) && isMain){
            ProcDeclarations(decl, blockName);
        }
        else {
            VarDeclarations(decl, blockName);

        }
    }
*/

    while (m_currentToken.m_type==TokenType::PROCEDURE || m_currentToken.m_type==TokenType::INTERRUPT || m_currentToken.m_type == TokenType::WEDGE  || m_currentToken.m_type==TokenType::FUNCTION || m_currentToken.m_type==TokenType::VAR) {
        if (m_currentToken.m_type==TokenType::VAR) {
            VarDeclarations(decl, blockName);
        }
        else
        {
            QVector<QSharedPointer<Node>> waste;
            ProcDeclarations(waste, blockName);
        }
    }



//    }
   // qDebug() << "Finally:" << m_currentToken.getType();
    if (m_currentToken.m_type!=TokenType::BEGIN && isMain && hasBlock)
        ErrorHandler::e.Error("After declarations, BEGIN is expected", m_currentToken.m_lineNumber);


//    m_inCurrentProcedure = "";

    return decl;
}

QVector<QSharedPointer<Node>> Parser::ConstDeclaration()
{
    Eat(TokenType::CONSTANT);
    QString name = m_currentToken.m_value;
    Eat();
    Eat(TokenType::COLON);
    QString type = "";
    if (m_currentToken.m_type == TokenType::ADDRESS)
        type="address";
    if (m_currentToken.m_type == TokenType::BYTE)
        type="byte";
    if (m_currentToken.m_type == TokenType::INTEGER)
        type="integer";
    if (m_currentToken.m_type == TokenType::LONG)
        type="long";
    TokenType::Type dType = m_currentToken.m_type;
    if (type=="") {
        ErrorHandler::e.Error("Unknown or illegal type when defining constant of type: '"+m_currentToken.m_value+"' ("+m_currentToken.getType()+")<br>Allowed types are : <b>address, byte, integer.</b> ",m_currentToken.m_lineNumber);
    }
    Eat();
    Eat(TokenType::EQUALS);
    int value = GetParsedInt(dType);
//    qDebug() << "DECLARING CONSTANT " <<name.toUpper();
    m_symTab->m_constants[name.toUpper()] = QSharedPointer<Symbol>(new Symbol(name.toUpper(),type.toUpper(),value));
    return QVector<QSharedPointer<Node>>();
}

QSharedPointer<Node>  Parser::TypeDeclaration()
{
    Eat(TokenType::TYPE);
    QString name = m_symTab->m_gPrefix+m_currentToken.m_value;
    Eat();
    Eat(TokenType::EQUALS);
    QSharedPointer<Node> typeSpec = TypeSpec(false,QStringList());
    if (m_types.contains(name))
        ErrorHandler::e.Error("Type already defined: "+name,m_currentToken.m_lineNumber);

    m_types[name] = typeSpec;

//    qDebug() << m_types.keys();

    return typeSpec;
}



QVector<QSharedPointer<Node> > Parser::VariableDeclarations(QString blockName, bool isProcedureParams)
{
    if (blockName!="")
        m_symTab->SetCurrentProcedure(blockName+"_");
    else
        m_symTab->SetCurrentProcedure("");


    int orgLineNumber = m_currentToken.m_lineNumber+1;
    m_currentToken.m_value = VerifyVariableName(m_currentToken.m_value);

    // All the vars that will be declared
    QVector<QSharedPointer<Node>> vars;
    // Make sure that prefix is added
    if (!m_symTab->isRegisterName(m_currentToken.m_value))
        m_currentToken.m_value = m_symTab->m_gPrefix+m_currentToken.m_value;
    //qDebug() << m_currentToken.m_value;
    vars.append(QSharedPointer<NodeVar>(new NodeVar(m_currentToken)));

    QString varName = m_currentToken.m_value;
    QStringList variableNames;
    variableNames << varName;
//    qDebug() << "CURRENT VARNAME "<< varName;
    QVector<QSharedPointer<Symbol>> syms;
    syms.append(QSharedPointer<Symbol>(new Symbol(m_currentToken.m_value,"")));
    Eat(TokenType::ID);

    // Declare a record?
    if (m_currentToken.m_type==TokenType::EQUALS) {
        Eat(TokenType::EQUALS);
        if (m_currentToken.m_type==TokenType::RECORD || m_currentToken.m_type==TokenType::CLASS) {
            return Record(varName);

        }
        ErrorHandler::e.Error("Unknown declaration : " +m_currentToken.m_value);
    }

    // Make sure that ALL are defined!

    while (m_currentToken.m_type == TokenType::COMMA) {
        Eat(TokenType::COMMA);
        // Prefix
        m_currentToken.m_value = VerifyVariableName(m_currentToken.m_value);

        if (!m_symTab->isRegisterName(m_currentToken.m_value))
          m_currentToken.m_value = m_symTab->m_gPrefix+m_currentToken.m_value;
        variableNames << m_currentToken.m_value;
        vars.append(QSharedPointer<NodeVar>(new NodeVar(m_currentToken)));
        AppendComment(vars[vars.count()-1]);
        syms.append(QSharedPointer<Symbol>(new Symbol(m_currentToken.m_value,"")));

        Eat(TokenType::ID);
    }
    Eat(TokenType::COLON);

    QStringList preflags  =getFlags(); // Allow for flags to be defined before the type
    bool isGlobal = preflags.contains("global");

    // NOW do the syms define
//    qDebug() << "PARSER "<<isGlobal << variableNames;
    if (!isGlobal)
    for (QSharedPointer<Symbol> s: syms) {
        if (Syntax::s.m_illegaVariableNames.contains(s->m_name))
            ErrorHandler::e.Error("Illegal variable name '" + s->m_name +"' on the "+AbstractSystem::StringFromSystem(Syntax::s.m_currentSystem->m_system)+" (name already used in the assembler etc) ",m_currentToken.m_lineNumber);
        //if (m_isRecord)
        //qDebug() << "Defining RECORD: " << s->m_name;
        QString sn = s->m_name;
        sn = sn.toLower();



        if (!m_symTab->isRegisterName(sn))
        {
            if (m_symTab->LookupConstants(s->m_name.toUpper())!=nullptr)
                ErrorHandler::e.Error("'" + s->m_name +"' is already defined as a constant.",m_currentToken.m_lineNumber);
/*            if (s->m_name.toLower().contains("keyrow"))
                qDebug() << m_symTab->m_constants.keys();
            qDebug() << s->m_name;
*/
            m_symTab->Define(s ,false);
        }

        s->m_lineNumber = m_currentToken.m_lineNumber;
    }

//    qDebug() << "CURVAL " <<m_currentToken.m_value;
    QSharedPointer<NodeVarType> typeNode = qSharedPointerDynamicCast<NodeVarType>(TypeSpec(isProcedureParams, variableNames));
    typeNode->m_flags.append(getFlags());
    typeNode->m_flags.append(preflags);
    typeNode->VerifyFlags(isProcedureParams);
    if (Syntax::s.m_currentSystem->m_system==AbstractSystem::GAMEBOY || Syntax::s.m_currentSystem->m_system==AbstractSystem::COLECO) {
        //if (typeNode->m_op.m_type==TokenType::POINTER)
        if (typeNode->m_data.count()<=1 && typeNode->m_op.m_type!=TokenType::INCBIN && typeNode->m_op.m_type!=TokenType::STRING)
        {
            typeNode->m_flags.append("wram"); // Always declare pointers in WRAM on GB
        }
    }
    // Set all types and array types, sizes
    for (auto& s: syms) {
       s->m_type = typeNode->m_op.m_value;
       s->m_flags = typeNode->m_flags;
       s->m_bank = typeNode->m_bank;


       s->m_arrayType = typeNode->m_arrayVarType.m_type;
       s->m_arrayTypeText = TokenType::getType(typeNode->m_arrayVarType.m_type);
//       qDebug() << "FLAGS " << s->m_name << " " <<s->m_type<<s->m_arrayTypeText;
       if (typeNode->m_arrayVarType.m_type==TokenType::RECORD) {
           s->m_arrayTypeText = typeNode->m_arrayVarType.m_value;
//           qDebug() << "TYPE RECORD "<<s->m_name<<  s->m_arrayTypeText;
  //         s->m_flags = typeNode->m_flags;

       }
       if (s->m_type=="POINTER")
            s->m_pointsTo = typeNode->m_arrayVarType.m_value;


        // Finally, calculate sizes
       int size = typeNode->m_declaredCount;
       if (typeNode->m_data.count()!=0) // Replace with actual data count
           size = typeNode->m_data.count();
       typeNode->m_declaredCount = size;

       s->setSizeFromCountOfData(size);



//        qDebug() << "Declaring :" <<s->m_name<<s->m_size;

    }
/*
//       qDebug()  << s->m_size <<typeNode->m_declaredCount;
       if (typeNode->m_data.count()!=0)
           s->m_size = typeNode->m_data.count();

       else {
           s->m_size = 1;
           if (s->m_type.toLower()=="integer")
               s->m_size =2;
           if (s->m_type.toLower()=="long")
               s->m_size =4;
       }
  */



    QVector<QSharedPointer<Node>> var_decleratons;
    if (isGlobal) { // Typecheck that they exist
        for (QSharedPointer<Node> n : vars) {
            QSharedPointer<NodeVar> v = qSharedPointerDynamicCast<NodeVar>(n);
            try {
               m_symTab->Lookup(v->value,v->m_op.m_lineNumber);
            } catch (FatalErrorException& fe) {
                fe.message = fe.message + "When using the <font color=\"yellow\">global</font> keyword, the variable must in question must be declared in the global variable scope. ";
                throw fe;
            }

        }
    }
    int add=0, cur=0;


   for (QSharedPointer<Node> n : vars) {
        QSharedPointer<NodeVarDecl> decl = QSharedPointer<NodeVarDecl>(new NodeVarDecl(n, typeNode));
        QSharedPointer<NodeVar> v = qSharedPointerDynamicCast<NodeVar>(n);
        if (m_symTab->isRegisterName(v->value)) {
            v->m_isRegister = true;
        }

        decl->m_op.m_lineNumber = orgLineNumber;
        v->m_isGlobal = isGlobal;
  //      if (!m_isRecord)
           var_decleratons.append(decl);
//        else

//        if (m_isRecord)
  //          qDebug() << v->value;
        // Update extra list for assembler
       if (typeNode->m_position!="") { // HAS a fixed position
           //qDebug() << "FX" <<v->value;
            m_symTab->m_extraAtSymbols[v->value] = Util::numToHex(Util::NumberFromStringHex(typeNode->m_position)+add);
            add+=syms[cur]->m_size;
//            qDebug() <<syms[cur]->m_value->m_strVal<<syms[cur]->m_size;
            cur++;

       }



//        qDebug() <<  typeNode->m_op.getType() << typeNode->m_op.m_value << (qSharedPointerDynamicCast<NodeVar>n)->value;;
        if (typeNode->m_op.m_type == TokenType::INCSID) {
            //            decl->m_isUsed = true;

            int sidloc = 0;
            if (m_preprocessorDefines.contains("SIDEmulatorLocation")) {
                sidloc = Util::NumberFromStringHex(m_preprocessorDefines["SIDEmulatorLocation"]);
                //qDebug() << sidloc;
            }
            //            exit(1);
            decl->InitSid(m_symTab,m_currentDir, sidloc, "sid");
        }
        if (typeNode->m_op.m_type == TokenType::INCNSF) {
            int sidloc = 0;
            //           exit(1);
            decl->InitSid(m_symTab, m_currentDir, sidloc, "nsf");
        }
   }

//    return vars;
    return var_decleratons;
}



QSharedPointer<Node> Parser::TypeSpec(bool isInProcedure, QStringList varNames)
{
    Token t = m_currentToken;

    if (m_types.contains(t.m_value) || m_types.contains(m_symTab->m_gPrefix+t.m_value)) {
        QString n = t.m_value;
        if (!m_types.contains(t.m_value))
            n = m_symTab->m_gPrefix+t.m_value;
        Eat();
        return m_types[n];
    }


    if (m_currentToken.m_type == TokenType::INCBIN || m_currentToken.m_type == TokenType::INCSID || m_currentToken.m_type == TokenType::INCNSF) {
        Eat();
//        qDebug() << Node::m_staticBlockInfo.m_blockPos;
        Eat(TokenType::LPAREN);
        QString binFile = m_currentToken.m_value;
        Eat();
        QString position ="";
        int start =0, len =0;
        bool requireSplit = false;
        if (m_currentToken.m_type==TokenType::COMMA) {
            Eat();
            position = Util::numToHex(GetParsedInt(TokenType::ADDRESS));
        }
        if (m_currentToken.m_type==TokenType::COMMA) {
            Eat();
            start = GetParsedInt(TokenType::LONG);
            len = -1;
            if (m_currentToken.m_type==TokenType::COMMA) {
                Eat(TokenType::COMMA);
                len = GetParsedInt(TokenType::LONG);
            }
            requireSplit = true;
        }
        Eat(TokenType::RPAREN);


        if (requireSplit) {
            QString f = m_currentDir+QDir::separator()+binFile;
            if (!QFile::exists(f))
               ErrorHandler::e.Error("Could not find file for inclusion: "+f, m_currentToken.m_lineNumber);
            QString of = Util::getFileWithoutEnding(f) + "_split.bin";
            QByteArray ba = Util::loadBinaryFile(f);
            if (len==-1)
                len = ba.size();
            QByteArray res = ba.mid(start,len);
            Util::SaveByteArray(res,of);
            binFile = Util::getFileWithoutEnding(binFile)+"_split.bin";
        }

 //       qDebug() << "LOOKING FOR FLAGS WITH INCBIN "<< binFile;
        QStringList flags = getFlags();
   //     qDebug() << "FLAGS FOUND :  "<< flags;

        QSharedPointer<NodeVarType> nt =  QSharedPointer<NodeVarType>(new NodeVarType(t,binFile, position));
        nt->m_flags = flags;
        nt->VerifyFlags(isInProcedure);
        return nt;


    }

    // Array [] of blah = (1,2,3);
    if (m_currentToken.m_type == TokenType::ARRAY) {
        Eat(TokenType::ARRAY);
        Eat(TokenType::LBRACKET);
        float count = 0;
        if (m_currentToken.m_type != TokenType::RBRACKET)
            count = GetParsedInt(TokenType::NADA);

        Eat(TokenType::RBRACKET);
        Eat(TokenType::OF);
        QStringList flags = getFlags();
        Token arrayType = m_currentToken;
        VerifyTypeSpec(arrayType);
        TokenType::Type dataType = m_currentToken.m_type;
        if (Syntax::s.m_currentSystem->m_processor == AbstractSystem::MOS6502 &&
                dataType == TokenType::POINTER) {
            QString ie = "<br><font color=\"grey\">Example:<br>anIntegerArray[i]:=#data;<br>aPointer := anIntegerArray[i];</font>";
            ErrorHandler::e.Error("Cannot have arrays of (zeropage) pointers on the 6502. Instead, please use an array of integers, and then assign a pointer to the (integer) array item."+ie,m_currentToken.m_lineNumber);
        }
        Eat(m_currentToken.m_type);
        flags<< getFlags();
        QStringList data;
        // Contains constant init?
        if (m_currentToken.m_type==TokenType::EQUALS) {
            Eat();
            if (m_currentToken.m_type==TokenType::BUILDTABLE) {
                data = BuildTable(count, dataType);
            }
            else {
                Eat(TokenType::LPAREN);
                while (m_currentToken.m_type!=TokenType::RPAREN) {
                    bool found = false;
                    // First check if symbol exists:
                    if (m_currentToken.m_value!="") {
                        if (m_symTab->m_symbols.contains(m_currentToken.m_value) && (m_symTab->LookupConstants(m_currentToken.m_value.toUpper())==nullptr)) {
                            data<<m_currentToken.m_value;
                            m_symTab->m_symbols[m_currentToken.m_value]->isUsed = true;
                            m_symTab->m_symbols[m_currentToken.m_value]->isUsedBy <<m_inCurrentProcedure;
                            found = true;
                        }
                        //                    qDebug() << "ADDRESS " << m_currentToken.m_value <<m_symTab->LookupConstants(m_currentToken.m_value.toUpper());
                    }
                    if (!found) {

                        data << "$0"+QString::number(GetParsedInt(dataType),16);//QString::number(m_currentToken.m_intVal);
                    }
                    //data << "$0"+QString::number(GetParsedInt(),16);//QString::number(m_currentToken.m_intVal);
                    if (m_currentToken.m_type!=TokenType::RPAREN) {

                        Eat();
                        if (m_currentToken.m_type==TokenType::COMMA)
                            Eat();
                    }
                }
            }

            QString vars = Util::toString(varNames);
            Eat(TokenType::RPAREN);

            if (count!=data.count() && count!=0) {
                if (count>data.count()) {
                    ErrorHandler::e.Warning("Declared array count for variable '"+vars+"' ("+QString::number((int)count)+") does not match with data ("+QString::number(data.count())+"). Padding with zeros. ", m_currentToken.m_lineNumber);
                    for (int i=data.count();i<count;i++)
                        data.append(QString("0"));
                }
                else
                    ErrorHandler::e.Warning("Declared array count for variable '"+vars+"' ("+QString::number((int)count)+") does not match with data ("+QString::number(data.count())+"). Adjusting array size to fit data. ", m_currentToken.m_lineNumber);

            }

        } // end of array

        QString position = "";
        if (m_currentToken.m_type==TokenType::AT || m_currentToken.m_type==TokenType::ABSOLUT) {
            Eat();

            //position = m_currentToken.getNumAsHexString();
            position = Util::numToHex(GetParsedInt(TokenType::ADDRESS));
//            qDebug() << "PARSER "<<position;


           // Eat(m_currentToken.m_type);
        }
//        QStringList flags = getFlags();

        t.m_intVal = count;
//        qDebug() << "Type: " << t.m_value;
  //      t.m_type = arrayType.m_type;
//        qDebug()<< "PARSE "<< arrayType.getType() <<arrayType.m_value;
        if (m_symTab->m_records.contains( arrayType.m_value)) {
            arrayType.m_type = TokenType::RECORD;
            if (m_symTab->m_records[arrayType.m_value]->ContainsArrays())
                if (!m_symTab->m_records[arrayType.m_value]->m_isClass)
                    ErrorHandler::e.Error("You cannot declare an array of records that contain sub-arrays due to 6502 limitations. <br>Please remove the sub-array from the record type in question : '"+arrayType.m_value+"'.",arrayType.m_lineNumber);
        }

        QSharedPointer<NodeVarType> nt =  QSharedPointer<NodeVarType>(new NodeVarType(t,position, arrayType,data));
        nt->m_flags = flags;
        nt->VerifyFlags(isInProcedure);
        nt->m_declaredCount = count;
        return nt;


    }

    if (m_currentToken.m_type == TokenType::STRING || m_currentToken.m_type==TokenType::CSTRING) {
        bool isCString = m_currentToken.m_type==TokenType::CSTRING;
        Eat();
        QStringList initData;
        QStringList flags = getFlags();

        if (m_currentToken.m_type == TokenType::EQUALS) {
            Eat();
            QSharedPointer<NodeString> str = qSharedPointerDynamicCast<NodeString>(String(isCString));
            initData = str->m_val;
        }
        QSharedPointer<NodeVarType> str = QSharedPointer<NodeVarType>(new NodeVarType(t,initData));
        str->m_flags = flags;
        str->VerifyFlags(isInProcedure);
        return str;
    }

    // ^byte, ^integer
    if (m_currentToken.m_isPointer) {
        if (m_isRecord && !Syntax::s.m_currentSystem->AllowPointerInStructs())
            ErrorHandler::e.Error("You cannot declare pointers in records/classes on this CPU type. Please use an integer to store the address instead, and assign a pointer to it when required.  ",m_currentToken.m_lineNumber);

        t.m_type = TokenType::POINTER;
        t.m_value = "POINTER";
        QSharedPointer<NodeVarType> nvt = QSharedPointer<NodeVarType>(new NodeVarType(t,""));
        nvt->m_arrayVarType.m_type = m_currentToken.m_type;

        if (!(m_currentToken.m_type==TokenType::INTEGER || m_currentToken.m_type==TokenType::BYTE || m_currentToken.m_type==TokenType::LONG  || isClass(m_currentToken))) {
            ErrorHandler::e.Error("Must point to either byte, integers, long (m68k/mega65).",m_currentToken.m_lineNumber);
        }
        Eat();
        if (m_currentToken.m_type == TokenType::EQUALS) {
            ErrorHandler::e.Error("On the 6502, pointers must be initialized through code. Z80/M68K pointer initialization not yet implemented in TRSE. ",m_currentToken.m_lineNumber);

        }
        nvt->VerifyFlags(isInProcedure);
        if (m_currentToken.m_type == TokenType::AT) {
            Eat();
            nvt->initVal = Util::numToHex(GetParsedInt(TokenType::ADDRESS));
    //        Eat();

        }
        return nvt;

    }

    if (m_currentToken.m_type == TokenType::POINTER) {
        if (m_isRecord && !Syntax::s.m_currentSystem->AllowPointerInStructs())
            ErrorHandler::e.Error("You cannot declare pointers in records/classes on this CPU type. Please use an integer to store the address instead, and assign a pointer to it when required.  ",m_currentToken.m_lineNumber);
//        QString type;
        Eat();

        QSharedPointer<NodeVarType> nvt = QSharedPointer<NodeVarType>(new NodeVarType(t,""));
        nvt->m_arrayVarType.m_type = TokenType::BYTE;

        if (m_currentToken.m_type == TokenType::OF) {
            Eat();
            TokenType::Type typ = m_currentToken.m_type;
            m_currentToken.m_value = m_symTab->m_gPrefix+ m_currentToken.m_value;
            VerifyTypeSpec(m_currentToken);
            QString val = m_currentToken.m_value;


            Eat();
            nvt->m_arrayVarType.m_type = typ;
            nvt->m_arrayVarType.m_value = val;
//            qDebug() <<"HERE points to " <<TokenType::getType(typ) <<val;

        }
        if (m_currentToken.m_type == TokenType::AT) {
            Eat();
            nvt->initVal = Util::numToHex(GetParsedInt(TokenType::ADDRESS));
//            Eat();

        }

//        qDebug() <<"Parser typespec pointer: "  << nvt->m_arrayVarType.getType();
        nvt->VerifyFlags(isInProcedure);
  //      qDebug() << "WTF "<<m_currentToken.getType();

        return nvt;
    }


    Eat();
    // Is regular single byte / pointer
    QString position = "";
    if (m_currentToken.m_type==TokenType::AT || m_currentToken.m_type==TokenType::ABSOLUT) {
        Eat();

        position = Util::numToHex(GetParsedInt(TokenType::ADDRESS));
//        qDebug() << "LOOKUP constnt : "<<position;

//        Eat(m_currentToken.m_type);
        QSharedPointer<NodeVarType> nt = QSharedPointer<NodeVarType>(new NodeVarType(t, position ));
//        qDebug() << "IN TYPESPEC : " << t.m_value << TokenType::getType(t.m_type);
        nt->m_position = position;
        // Only declare as CONST if NOT a record
        nt->m_flag = 1;
//        VerifyTypeSpec(t);
        nt->VerifyFlags(isInProcedure);

        return nt;
    }


    QString initVal = "";
    if (m_currentToken.m_type == TokenType::EQUALS) {
        Eat();
//        data << "$0"+QString::number(getIntVal(m_currentToken),16);//QString::number(m_currentToken.m_intVal);

        initVal = Util::numToHex(GetParsedInt(t.m_type));
        if (Syntax::s.m_currentSystem->m_system == AbstractSystem::NES || Syntax::s.m_currentSystem->m_system == AbstractSystem::GAMEBOY || Syntax::s.m_currentSystem->m_system == AbstractSystem::COLECO)
            ErrorHandler::e.Error("You cannot initialize variables stored in work ram (WRAM) on the Gameboy/NES, as these needs to be initialized manually. " ,m_currentToken.m_lineNumber-1);



    }
    VerifyTypeSpec(t);

    if (t.m_type == TokenType::BOOLEAN) {
//        qDebug() << "Replace bool with byte";
        t.m_value="BYTE";
       t.m_type = TokenType::BYTE;
    }

    QSharedPointer<NodeVarType> nvt = QSharedPointer<NodeVarType>(new NodeVarType(t,initVal));
    nvt->VerifyFlags(isInProcedure);

    return nvt;

}

QSharedPointer<Node> Parser::BuiltinFunction()
{
    if (Syntax::s.builtInFunctions.contains(m_currentToken.m_value.toLower())) {
        QString procName = m_currentToken.m_value.toLower();
        int noParams = Syntax::s.builtInFunctions[procName].m_params.count();
        Eat(TokenType::ID);
        Eat(TokenType::LPAREN);
        QVector<QSharedPointer<Node>> paramList;
        QString prev;

        while (m_currentToken.m_type!=TokenType::RPAREN) {
            if (m_currentToken.m_type==TokenType::STRING) {
                paramList.append( String() );
            }
            else {
                QString pname = m_currentToken.m_value;
                try {
                    paramList.append(Expr());
                } catch (FatalErrorException& fe) {
                    QString em = "Could not find symbol '<font color=\"#FF8000\">" + prev + "</font>'<br>";
                    QString similarSymbol = m_symTab->findSimilarSymbol(prev,65,2,m_procedures.keys());
                    if (similarSymbol!="") {
                        em+="Did you mean '<font color=\"#A080FF\">"+similarSymbol+"</font>'?<br>";
                    }
                    fe.message = em + fe.message;

                    throw fe;
                }
                prev = pname;
            }

            if (m_currentToken.m_type==TokenType::COMMA)
                Eat(TokenType::COMMA);
        }

        Eat(TokenType::RPAREN);

        //qDebug() << "Params count for " << procName << " :" << paramList.count();
        if (noParams!=paramList.count()) {
            QString s = "Error using builtin function " + procName + " \n";
            s += "Requires " + QString::number(noParams) + " parameters but has " + QString::number(paramList.count());

            ErrorHandler::e.Error(s, m_currentToken.m_lineNumber);
        }

        // Give obsolete warnings
        for (QStringList& lst: m_obsoleteWarnings) {
                if(lst[0].toLower() == procName.toLower()) {
                    if (!m_warningsGiven.contains(procName)) {
                      m_warningsGiven.append(procName);
                      ErrorHandler::e.Warning(lst[1], m_currentToken.m_lineNumber);
                     }
                 }
            }

        return QSharedPointer<NodeBuiltinMethod>(new NodeBuiltinMethod(procName,paramList,&Syntax::s.builtInFunctions[procName]));
        //p->SetParameters(paramList);


    }
    return nullptr;

}

QSharedPointer<Node> Parser::Constant()
{
    QString id = m_currentToken.m_value;
    if (m_symTab->m_constants.contains(id)) {
        Eat(m_currentToken.m_type);
        QSharedPointer<Symbol> s = m_symTab->m_constants[id];
        QSharedPointer<Node> n =  QSharedPointer<NodeNumber>(new NodeNumber(Token(s->getTokenType(), s->m_value->m_fVal), s->m_value->m_fVal));
        return n;
    }
    return nullptr;
}

QSharedPointer<Node> Parser::InlineAssembler()
{
    Token t = m_currentToken;
    Eat(TokenType::ASM);
    bool pascalStyleAsm = false;
    // Original pascal-style ASM without (" ");
    if (m_currentToken.m_type!=TokenType::LPAREN){
        //qDebug() <<m_currentToken.m_value;
        QString org = m_currentToken.m_value;
        m_currentToken = m_lexer->InlineAsm();
        //qDebug() <<m_currentToken.m_value;
        m_currentToken.m_value = "\t"+org + m_currentToken.m_value;
        pascalStyleAsm = true;

    }
    else {
        Eat(TokenType::LPAREN);
        if (m_currentToken.m_type!=TokenType::STRING)
            ErrorHandler::e.Error("Inline assembler must be enclosed as a string");

    }
    if (Syntax::s.m_currentSystem->m_processor==AbstractSystem::MOS6502) {
        VerifyInlineSymbols6502(m_currentToken.m_value);
    }
    t.m_value = m_currentToken.m_value;
    QSharedPointer<Node> n = QSharedPointer<NodeAsm>(new NodeAsm(t));
    Eat(TokenType::STRING);
    if (pascalStyleAsm) {
        Eat(TokenType::END);
    }
    else {
        Eat(TokenType::RPAREN);
    }
    return n;
}


void Parser::HandleImportChar()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    int param1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param2 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    LImage* imgB = LImageIO::Load(outFile);

    LImage* imgA = nullptr;
    if (inFile.toLower().endsWith(".bin") || inFile.toLower().endsWith(".chr")) {
        imgA = LImageFactory::Create(imgB);
        QFile f(inFile);
        f.open(QFile::ReadOnly);
        imgA->ImportBin(f);
        f.close();
    }
    if (inFile.toLower().endsWith(".flf"))
      imgA = LImageIO::Load(inFile);

    if (imgA == nullptr) {
        ErrorHandler::e.Error("Importing char error : unknown filetype for '"+inFile +"'");
    }

//    qDebug() << "HERE" << param1 << param2;
    imgB->CopySingleChar(imgA, param1, param2);

    LImageIO::Save(outFile,imgB);


}

QStringList Parser::BuildTable(int cnt,TokenType::Type type)
{
    Eat(TokenType::BUILDTABLE);
    if (cnt==0)
        ErrorHandler::e.Error("BuildTable must have at least 1 element in array.",m_currentToken.m_lineNumber);


    Eat(TokenType::LPAREN);
    QString sentence = m_currentToken.m_value;
    Eat(TokenType::STRING);
    QStringList data;
    QJSEngine m_jsEngine;
    int AND = 0xFFFF;
//    qDebug() << "PARSER " <<TokenType::getType(type);
    if (type==TokenType::BYTE)
        AND = 0xFF;
    if (type==TokenType::LONG)
        AND = 0xFFFFFFFF;

    QString consts = "";
    for (QString key:m_symTab->m_constants.keys())
        consts +=key+"="+QString::number(m_symTab->m_constants[key]->m_value->m_fVal)+";";


    for (int i=0;i<cnt;i++) {
        QString str = sentence;
//        str = str.replace("i",QString::number(i));
//        QJSValue ret = m_jsEngine.evaluate(str);
        QJSValue fun = m_jsEngine.evaluate("(function(i) { "+consts+";return "+str+"; })");
        if (fun.isError())
            ErrorHandler::e.Error("Error evaluation javascript expression : " + fun.toString() + " <br><br>", m_currentToken.m_lineNumber);

        QJSValueList args;
        args << i;
        QJSValue ret = fun.call(args);


        if (ret.isError())
            ErrorHandler::e.Error("Error evaluation javascript expression : " + ret.toString() + " <br><br>", m_currentToken.m_lineNumber);

//        data << Util::numToHex(ret.toInt()&0xFF);
 //       if ()
//        data << Util::numToHex(ret.toInt()&0xFF);
//        qDebug() <<  ret.toInt();
        data << Util::numToHex(ret.toInt()&AND);
    }

    return data;
}

void Parser::HandleExportPalette()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);

    if (!QFile::exists(inFile)) {
        ErrorHandler::e.Error("File not found : "+inFile,ln);
    }
    LImage* img = LImageIO::Load(inFile);
    if (QFile::exists(outFile))
        QFile::remove(outFile);


    img->ExportRGB8Palette(outFile);

}

void Parser::HandleSetCompressionWeights()
{
    double param1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    double param2 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    double param3 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    SSIM::weight_contrast = param3/100.0;
    SSIM::weight_structure = param2/100.0;
    SSIM::weight_luminosity = param1/100.0;
}

void Parser::HandleMacro()
{
    LMacro m;
    QString name = m_currentToken.m_value;
    Eat(TokenType::STRING);
    m.noParams = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    m.str = m_currentToken.m_value;
    Eat(TokenType::STRING);
    m_macros[name.toLower()] = m;
//    qDebug() << "MACRO "<<m.str;

}

void Parser::HandlePerlinNoise()
{
    QString name = m_currentToken.m_value;
    QString file = m_currentDir+"/"+ name;
    Eat(TokenType::STRING);
    int w = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int h = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int oct = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    float scale = m_currentToken.m_intVal;
    Eat();
    float pers = m_currentToken.m_intVal/100.0;
    Eat();
    int amp = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    SimplexNoise sn;
    sn.CreateNoiseData(file,w,h,oct,pers,scale,amp);

}

void Parser::HandleCallMacro(QString name, bool ignore)
{
    //qDebug() << "Calling macro " << name << m_currentToken.m_value;
    Eat();
    //qDebug() << "mac " << name << m_currentToken.m_value;
    Eat(TokenType::LPAREN);
    QStringList params;
    QString p;
    //qDebug() << "Before " <<m_pass <<m_currentToken.m_value << m_lexer->m_pos << m_lexer->m_text[m_lexer->m_pos];
    // Build the parameter list + "p"
    for (int i=0;i<m_macros[name].noParams;i++) {
        QString val = m_currentToken.m_value;
        if (val=="")
            val = QString::number(getParsedNumberOrConstant());
        Eat();
        params<<val;
        p+="p"+QString::number(i);
        //       qDebug() << val <<m_currentToken.m_value;
        if (m_currentToken.m_type==TokenType::COMMA) {
            Eat();
            p+=",";
        }
    }
    uint pos = m_lexer->m_pos+1;
    Eat(TokenType::RPAREN);
    if (m_currentToken.m_type == TokenType::SEMI)
        Eat(TokenType::SEMI);

    //  qDebug() << m_pass;
    // Ignore calling macros in pass 1. Or perhaps pass 0? hm
    if (ignore)
        return;

    /*    QString consts = "";
    for (QString key:m_symTab->m_constants.keys())
        consts +=key+"="+QString::number(m_symTab->m_constants[key]->m_value->m_fVal)+";";
*/

    //     QJSValue fun = m_jsEngine.evaluate("(function("+p+") { "+consts+";return "+str+"; })");
    //    qDebug() << m_macros[name].str;
    // Construct the javascript macro
    QJSValue fun = m_jsEngine.evaluate("__oo = ''; "
                                     "\n function Writeln(__v) {__oo=__oo+__v + '\\n'; } "
                                     "\n function writeln(__v) {__oo=__oo+__v + '\\n'; } "
                                     "\n function Write(__v) {__oo=__oo+__v; } "
                                     "\n function write(__v) {__oo=__oo+__v; } "
                                     "\n   (function("+p+") { "+m_macros[name].str+"; return __oo;})");
    if (fun.isError())
        ErrorHandler::e.Error("Error evaluation javascript expression : " + fun.toString() + " <br><br>", m_currentToken.m_lineNumber);

    QJSValueList args;
    // Add parameters
    for (QString p: params)
        args << p;

    // Execute javascript
    QJSValue ret = fun.call(args);

    if (ret.isError())
        ErrorHandler::e.Error("Error evaluation javascript expression : " + ret.toString() + " <br><br>", m_currentToken.m_lineNumber);

    // Inject macro text into the source code
    m_lexer->m_text.insert(pos,ret.toString());
    //       m_lexer->m_pos -=1;
    //       qDebug() << "TEXT" <<m_lexer->m_text;


}

void Parser::HandleExecute() {
    QString exec = m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString pstr =m_currentToken.m_value;
    QStringList params = pstr.trimmed().simplified().split(" ");
    Eat(TokenType::STRING);
    QString out;
    Syntax::s.m_currentSystem->StartProcess(exec,params,out,true,m_currentDir);


}

void Parser::HandleExport()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);



    int param1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param2 = 0;
    if (m_currentToken.m_type==TokenType::INTEGER_CONST) {
        param2 = m_currentToken.m_intVal;
        Eat(TokenType::INTEGER_CONST);

    }
    else {
        param2 = param1;
        param1 = 0;
    }


    if (!QFile::exists(inFile)) {
        ErrorHandler::e.Error("File not found : "+inFile,ln);
    }

    if (inFile.toLower().endsWith(".trt")) {
        TTRFile ttr;
        ttr.Load(inFile);
        ttr.Export(outFile, param2);
        return;
    }


    LImage* img = LImageIO::Load(inFile);
    if (dynamic_cast<CharsetImage*>(img)!=nullptr) {
        img->m_exportParams["Start"] = param1;
        img->m_exportParams["End"] = param2;
    }
    if (dynamic_cast<LImageNES*>(img)!=nullptr) {
        img->m_exportParams["Start"] = param1;
        img->m_exportParams["End"] = param2;
    }
    img->m_exportParams["export1"] = param2;

    if (QFile::exists(outFile))
        QFile::remove(outFile);



    QFile file(outFile);

    file.open(QFile::WriteOnly);
    img->m_silentExport = true;


    if (dynamic_cast<C64FullScreenChar*>(img)!=nullptr) {
        C64FullScreenChar* c = dynamic_cast<C64FullScreenChar*>(img);
        c->ExportMovie(file);
    }
    else
        img->ExportBin(file);


    file.close();

}

void Parser::HandleCompress()
{
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);

    QString lz4 = m_settingsIni->getString("lz4");
    if (!QFile::exists(lz4))
        ErrorHandler::e.Error("In order to compress files, please set up the 'lz4' path in the 'Utilities' section in the TRSE settings panel.", m_currentToken.m_lineNumber);

    if (QFile::exists(outFile))
        QFile::remove(outFile);
    QString out, err;
    QStringList params = QStringList() << "-l" << inFile << outFile;
    Syntax::s.m_currentSystem->StartProcess(lz4,params,out,true);

}

void Parser::HandleExportParallaxData()
{
//    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);

    int x0 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    int y0 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    int x1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    int y1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    int param1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    int param2 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    int param3 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    Compression::GenerateParallaxData(inFile, outFile, x0,y0,x1,y1,param1,param2, param3);


}

void Parser::HandleExportBW()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);



    int x = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int y = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int w = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int h = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    if (!QFile::exists(inFile))
        ErrorHandler::e.Error("Could not find file :" +inFile,m_currentToken.m_lineNumber);

    LImage* img = LImageIO::Load(inFile);
    if (QFile::exists(outFile))
        QFile::remove(outFile);


    QFile file(outFile);

    file.open(QFile::WriteOnly);
    img->m_silentExport = true;
    img->ExportBlackWhite(file,x,y,w,h);




    file.close();

}

void Parser::HandleExportCompressed()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);



    int x0 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    int y0 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    int w = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    int h = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    int c = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    if (!QFile::exists(inFile)) {
        ErrorHandler::e.Error("File not found : "+inFile,ln);
    }
    LImage* img = LImageIO::Load(inFile);
    img->m_exportParams["StartX"] = x0;
    img->m_exportParams["EndX"] = w;
    img->m_exportParams["StartY"] = y0;
    img->m_exportParams["EndY"] = h;
    img->m_exportParams["Compression"] = c;



    QString screenFile = Util::getFileWithoutEnding(outFile) +"_screen.bin";
    QString charFile = Util::getFileWithoutEnding(outFile) +"_charset.bin";
    QString colorFile = Util::getFileWithoutEnding(outFile) +"_color.bin";

//    QFile file(outFile);

  //  file.open(QFile::WriteOnly);
    img->m_silentExport = true;
    img->ExportCompressed(screenFile, charFile, colorFile);
//    if (img->m_exportMessage!="")
  //      ErrorHandler::e.Warning(img->m_exportMessage,m_currentToken.m_lineNumber);



//    file.close();

}




void Parser::HandleBuildPaw()
{
#ifndef CLI_VERSION
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value; //    QString inFile = m_currentToken.m_value;form
    Eat(TokenType::STRING);
    FormPaw fp;
    fp.InitDocument(nullptr,m_settingsIni,m_projectIni);
    if (!QFile::exists(inFile))
        ErrorHandler::e.Error("Could not locate paw file for building: "+inFile, ln);
//    fp.Load(inFile);
    fp.m_pawData->Load(inFile);

    fp.BuildSingle();
#endif
}

void Parser::HandleExportPrg2Bin()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    int from = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int to = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);

    QByteArray in  = Util::loadBinaryFile(inFile);
    QByteArray out;
    if (outFile.toLower().contains(".prg")) {
        out.append((char)((from)&0xFF)); // lo byte
        out.append((char)((from>>8)&0xFF)); // hi byte
    }
    int start = (in[0] | ((int)(in[1])<<8))&0xFFFF;
//    qDebug() << "PRG2BIN START "<<Util::numToHex(start);
    in = in.remove(0,2);
  //  qDebug() << "PRG2BIN "<<Util::numToHex(from) << Util::numToHex(to);
    for (int i=from;i<to;i++) {
        int j = i-start;
        if (in.count()<j)
            out.append((char)0);
        else
  //          ErrorHandler::e.Error("ExportPrg2Bin error: .prg file does not contain specified binary range.", m_currentToken.m_lineNumber);
            out.append(in[j]);
    }
    Util::SaveByteArray(out,outFile);
}

void Parser::HandlePBMExport()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    int param1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param2 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param3 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param4 = m_currentToken.m_intVal;

    if (!QFile::exists(inFile)) {
        ErrorHandler::e.Error("File not found : "+inFile,ln);
    }
    LImage* img = LImageIO::Load(inFile);
    if (QFile::exists(outFile))
        QFile::remove(outFile);

    QFile file(outFile);

    file.open(QFile::WriteOnly);
    img->m_silentExport = true;

    img->PBMExport(file, param1,param2,param3,param4);

    file.close();

}

void Parser::HandleVBMExport()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    int param1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param2 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param3 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param4 = m_currentToken.m_intVal;

    if (!QFile::exists(inFile)) {
        ErrorHandler::e.Error("File not found : "+inFile,ln);
    }
    LImage* img = LImageIO::Load(inFile);
    if (QFile::exists(outFile))
        QFile::remove(outFile);

    QFile file(outFile);

    file.open(QFile::WriteOnly);
    img->m_silentExport = true;

    img->VBMExport(file, param1,param2,param3,param4);

    file.close();

}

void Parser::HandleVBMExportColor()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    int param1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param2 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param3 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param4 = m_currentToken.m_intVal;

    if (!QFile::exists(inFile)) {
        ErrorHandler::e.Error("File not found : "+inFile,ln);
    }
    LImage* img = LImageIO::Load(inFile);
    if (QFile::exists(outFile))
        QFile::remove(outFile);

    QFile file(outFile);

    file.open(QFile::WriteOnly);
    img->m_silentExport = true;

    img->VBMExportColor(file, param1,param2,param3,param4);

    file.close();

}

void Parser::HandleVBMExportChunk()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    int param1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param2 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param3 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param4 = m_currentToken.m_intVal;

    if (!QFile::exists(inFile)) {
        ErrorHandler::e.Error("File not found : "+inFile,ln);
    }
    LImage* img = LImageIO::Load(inFile);
    if (QFile::exists(outFile))
        QFile::remove(outFile);

    QFile file(outFile);

    file.open(QFile::WriteOnly);
    img->m_silentExport = true;

    img->VBMExportChunk(file,param1,param2,param3,param4);

    file.close();

}

void Parser::HandleExportFrame()
{
    int ln = m_currentToken.m_lineNumber;
    QString inFile = m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    QString outFile =m_currentDir+"/"+ m_currentToken.m_value;
    Eat(TokenType::STRING);
    int param1 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param2 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param3 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param4 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param5 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param6 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param7 = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST);
    int param8 = m_currentToken.m_intVal;

    if (!QFile::exists(inFile)) {
        ErrorHandler::e.Error("File not found : "+inFile,ln);
    }
    LImage* img = LImageIO::Load(inFile);
    if (QFile::exists(outFile))
        QFile::remove(outFile);

    QFile file(outFile);

    file.open(QFile::WriteOnly);
    img->m_silentExport = true;

    img->ExportFrame(file,param1,param2,param3,param4,param5,param6,param7,param8);

    file.close();

}


void Parser::HandleAKGCompiler()
{
    QString filename = m_currentToken.m_value;
    Eat(TokenType::STRING); // Filename
    int addr = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST); // X

    if (m_pass!=PASS_PRE)
        return;

    if (!Tool::AKGCompiler(m_currentDir+filename,addr,m_symTab.get()))
        ErrorHandler::e.Error("Could not find music for inclusion : "+filename+".asm");
}

void Parser::HandleSpriteCompiler()
{

    QString filename = m_currentToken.m_value;
    Eat(TokenType::STRING); // Filename
    QString name = m_currentToken.m_value;
    Eat(TokenType::STRING); // Name
    int x = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST); // X
    int y = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST); // Y
    int w = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST); // W
    int h = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST); // H

    QString id = "drawsprite_cga_"+name;

    if (m_pass!=PASS_PRE)
        return;

//    if (Syntax::s.builtInFunctions.contains(id))
  //      return;

    LImage* img = LImageIO::Load(m_currentDir +"/"+filename);
    m_parserAppendix << img->SpriteCompiler(name,"","",x,y,w,h);



    QList<BuiltInFunction::Type> paramList;
    paramList<<BuiltInFunction::ADDRESS;
    paramList<<BuiltInFunction::ADDRESS;
    paramList<<BuiltInFunction::INTEGER;
    paramList<<BuiltInFunction::INTEGER;

    Syntax::s.builtInFunctions[id] = BuiltInFunction(id, paramList);


    delete img;
}

void Parser::HandleSpritePacker()
{
    QString inFile = m_currentDir+ m_currentToken.m_value;
    Eat(TokenType::STRING); // Filename

    QString outChrFileName = m_currentDir+m_currentToken.m_value;
    Eat(TokenType::STRING); // Filename

    QString outSpriteFileName = m_currentDir+m_currentToken.m_value;
    Eat(TokenType::STRING); // Filename

    QString type = m_currentToken.m_value.toLower();
    Eat(TokenType::STRING); // Filename

    int x = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST); // X
    int y = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST); // Y
    int w = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST); // W
    int h = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST); // H
    int comp = m_currentToken.m_intVal;
    Eat(TokenType::INTEGER_CONST); // H
    if (m_pass!=PASS_PRE)
        return;

    LImage* imgChrOut = nullptr;
    LImage* imgSrc = nullptr;

    if (QFile::exists(outChrFileName)) {
        imgChrOut = LImageIO::Load(outChrFileName);
    }
    else {
        if (type=="gameboy")
            imgChrOut = LImageFactory::Create(LImage::GAMEBOY, LColorList::NES);
    }
    if (imgChrOut == nullptr)
        ErrorHandler::e.Error("Unknown image type : "+type+". For now, only 'gameboy' is supported.", m_currentToken.m_lineNumber);

    if (inFile.toLower().endsWith(".bin") || inFile.toLower().endsWith(".chr")) {
        imgSrc = LImageFactory::Create(imgChrOut);
        QFile f(inFile);
        f.open(QFile::ReadOnly);
        imgSrc->ImportBin(f);
        f.close();
    }
    if (inFile.toLower().endsWith(".flf"))
      imgSrc = LImageIO::Load(inFile);

    if (imgSrc == nullptr) {
        ErrorHandler::e.Error("Importing char error : unknown filetype for input binary '"+inFile +"'");
    }

    QByteArray spriteData;

    if (QFile::exists(outSpriteFileName))
        spriteData = Util::loadBinaryFile(outSpriteFileName);

    int curPos = spriteData.count();

    imgChrOut->SpritePacker(imgSrc, spriteData, x,y,w,h,comp);

    Util::SaveByteArray(spriteData, outSpriteFileName);
    LImageIO::Save(outChrFileName,imgChrOut);
    ErrorHandler::e.Warning("Added new sprite data from '"+inFile+"' : sprite from "+QString::number(curPos) + "  to " + QString::number(spriteData.count()),m_currentToken.m_lineNumber);
//    qDebug() << "SPRDATA " <<spriteData;

}

void Parser::HandleProjectSettingsPreprocessors()
{
    QString cmd = m_currentToken.m_value.toLower();
    Eat(TokenType::STRING); // Filename
    QString val = m_currentToken.m_value;
    if (val=="")
       val = QString::number(m_currentToken.m_intVal);
    if (cmd == "startaddress") {
        m_projectIni->setFloat("override_target_settings", 1);
        m_projectIni->setString("override_target_settings_org", val);
        return;
    }
    if (cmd == "basicsysaddress") {
        m_projectIni->setFloat("override_target_settings", 1);
        m_projectIni->setString("override_target_settings_basic", val);
        return;
    }
    if (cmd == "ignorebasicsysstart") {
        m_projectIni->setFloat("override_target_settings", 1);
        m_projectIni->setFloat("override_target_settings_sys", val.toInt());
        return;
    }

    if (cmd == "stripprg") {
        m_projectIni->setFloat("override_target_settings", 1);
        m_projectIni->setFloat("override_target_settings_prg", val.toInt());
        return;
    }
    if (cmd == "ignorejmp") {
        m_projectIni->setFloat("override_target_settings", 1);
        m_projectIni->setFloat("ignore_initial_jump", val.toInt());
        return;
    }
    if (cmd == "petmodel") {
        m_projectIni->setString("petmodel", val);
        Syntax::s.m_currentSystem->InitSystemPreprocessors(m_preprocessorDefines);
        return;
    }
    if (cmd == "amstradcpc_model") {
        m_projectIni->setString("amstradcpc_model", val);
        return;
    }
    if (cmd == "amstradcpc_options") {
        m_projectIni->setString("amstradcpc_options", val);
        return;
    }
    if (cmd == "exomize") {
        m_projectIni->setFloat("exomizer_toggle", val.toInt());
        return;
    }
    if (cmd == "system") {
        m_projectIni->setString("system", val);
        return;
    }

    Eat(); // H
}

/*void Parser::HandleExecute()
{
    QStringList ls = m_currentToken.m_value.split(" ");
    Eat();
    QString f = ls.first();
    ls.removeAll(f);
    QString out;
    Syntax::s.m_currentSystem->StartProcess(f,ls,out);
}
*/

void Parser::HandleUseTPU(QString fileName)
{

    if (s_usedTRUs.contains(fileName)) {
        return;
    }

    QStringList dirs;
    dirs << m_currentDir + QDir::separator();
    dirs << Util::GetSystemPrefix()+ Data::data.unitPath + QDir::separator()+AbstractSystem::StringFromSystem(Syntax::s.m_currentSystem->m_system)+ QDir::separator();
    dirs << Util::GetSystemPrefix()+ Data::data.unitPath + QDir::separator()+Data::data.cpuUnitPath+QDir::separator()+AbstractSystem::StringFromProcessor(Syntax::s.m_currentSystem->m_processor)+ QDir::separator();
    dirs << Util::GetSystemPrefix()+ Data::data.unitPath + QDir::separator()+"global"+ QDir::separator();

    QString fname = Util::findFileInDirectories(fileName + ".tru", dirs);


    if (fname == "" || !QFile::exists(fname)) {
        ErrorHandler::e.Error("Could not find TRU file for inclusion : "+fileName + " <br><br>( Tried dirs: " + Util::toString(dirs) +")",m_currentToken.m_lineNumber);
    }
    s_usedTRUs.append(fileName);
//    qDebug() << "ADDING" << fileName;

    QSharedPointer<Parser> p = QSharedPointer<Parser>(new Parser);
    QSharedPointer<Lexer> l = QSharedPointer<Lexer>(new Lexer);
    p->m_lexer = l;
    p->m_currentDir= m_currentDir;
    l->m_orgText = l->m_text;
    p->m_isTRU = true;
    p->m_projectIni = m_projectIni;
    p->m_settingsIni = m_settingsIni;
 //   p->m_tpus = m_tpus;
    p->m_symTab = QSharedPointer<SymbolTable>(new SymbolTable);
    p->m_symTab->m_globalList = m_symTab->m_globalList;


    p->m_symTab->m_symbols = m_symTab->m_symbols;
    p->m_symTab->m_records = m_symTab->m_records;
    p->m_symTab->m_externalRecords = m_symTab->m_records.keys();

//    qDebug() << "Copying over: " << m_procedures.keys();
    p->m_procedures = m_procedures;

    p->m_preprocessorDefines = m_preprocessorDefines;
    l->m_text = Util::loadTextFile(fname);
    l->m_text = l->m_text.replace("\r\n","\n");
    l->m_orgText = l->m_text;
    p->m_currentFileShort = fname;
    p->m_ignoreMethods = m_ignoreMethods;

    try {

        p->m_tree = p->Parse( false
                             ,m_vicMemoryConfig,
                             Util::fromStringList(m_projectIni->getStringList("global_defines")),
                             m_projectIni->getdouble("pascal_settings_use_local_variables")==1.0);
    } catch (FatalErrorException& e)
    {
        e.message = "<font color=\"#FFB030\">Error during compiling the Turbo Rascal Unit file : '</font><font color=\"yellow\">" +fileName + "</font>'<font color=\"#FFB030\"> on line " +QString::number(e.linenr)+ "</font><br><font color=\"red\">"+e.message+"</font>";
        e.linenr = m_currentToken.m_lineNumber;
       throw e;
    }


    m_symTab->Merge(p->m_symTab.get(),true);
    // Merge types as well
    for (QString s: p->m_types.keys()) {
        m_types[s] = p->m_types[s];
    }
    m_doNotRemoveMethods.append(p->m_doNotRemoveMethods);
    m_ignoreBuiltinFunctionTPU.append(p->m_ignoreBuiltinFunctionTPU);
//    m_userBlocks.append(p->m_userBlocks);

    m_userBlocks.append(p->m_userBlocks);

    for (QString s : p->m_symTab->m_symbols.keys()) {
        m_symTab->m_globalList.append(s);
    }
   // qDebug() << "LIST : " <<m_symTab->m_symbols.keys();
    if (!m_tpus.contains(p))
        m_tpus.append(p);

//    qDebug() << m_currentToken.m_value;
//    Eat();
//    qDebug() << m_currentToken.m_value;
}

QSharedPointer<Node> Parser::Expr()
{
    QSharedPointer<Node> node = Term();




    while (m_currentToken.m_type == TokenType::Type::PLUS || m_currentToken.m_type == TokenType::Type::MINUS
            || m_currentToken.m_type == TokenType::Type::BITAND || m_currentToken.m_type == TokenType::Type::BITOR) {
        Token t = m_currentToken;

        Eat(m_currentToken.m_type);

        node = QSharedPointer<NodeBinOP>(new NodeBinOP(node, t, Term()));

    }

    if (node->isPureNumeric() && qSharedPointerDynamicCast<NodeNumber>(node)==nullptr) {
        // Calculate and COLLAPSE. Easier on the dispatcher.
        int val = node->getValueAsInt(nullptr);
        Token t;
        t.m_type = node->VerifyAndGetNumericType();
        t.m_lineNumber = node->m_op.m_lineNumber;
        t.m_isReference = node->isReference();
//        qDebug() << "IS REFERENCE" <<val <<t.m_isReference;
//        qDebug() << "Collapsing node to " << Util::numToHex(val) << t.getType() << t.m_lineNumber << node->m_op.getType();

        return QSharedPointer<NodeNumber>(new NodeNumber(t,val));

//        qDebug() << node->m_op.getType();
//        t.m_type = TokenType::CON
    }
/*
 *
 *  Experimental: replace DIV with SHR
    if (qSharedPointerDynamicCast<NodeBinOP>(node)!=nullptr)
    if (node->m_op.m_type==TokenType::DIV) {
        if (node->m_right->isPureNumeric()) {
            int val = node->m_right->getValueAsInt(nullptr);
            if (div2s.contains(val)) {
                node->m_op.m_type = TokenType::SHR;
                qSharedPointerDynamicCast<NodeNumber>(node->m_right)->m_val = div2s.indexOf(val)+1;
            }
        }
    }
*/


    return node;
}

