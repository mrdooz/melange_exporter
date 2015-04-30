#include <c4d_file.h>
#include <c4d_ccurve.h>
#include <c4d_ctrack.h>

#include "exporter.hpp"
#include <stdint.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
// know application IDs

// Vectorworks
#define APPLICATION_ID_VECTORWORKS    310003000 

// Allplan
#define MELANGE_ALLPLAN_APPLICATION_ID 1026284
#define AllplanElementID		1021906	// (deprecated)
#define AllplanAllrightID		1021912	// (deprecated)

// ArchiCAD
#define APPLICATION_ID_ARCHICAD			1026292
#define APPLICATION_ID_ARCHICAD_EX	2886976735	// (deprecated)
#define ID_ISARCHICAD		465001600 // Bool		(deprecated)
#define ID_ARCHICADNAME	465001601 // String (deprecated)

uint64_t tempmatid = 0;
uint64_t tempLayID = 0;

namespace melange
{
  void PrintUniqueIDs(BaseList2D *op);
  void PrintShaderInfo(BaseShader *shader, int depth = 0);
  void PrintAnimInfo(BaseList2D *bl);
  void PrintTagInfo(BaseObject *obj);

  void AlienPolygonObjectData::Print()
  {
    // get object pointer
    PolygonObject *op = (PolygonObject*)GetNode();

    GeData data;

    // get point and polygon array pointer and counts
    const Vector *vAdr = op->GetPointR();
    int pointcnt = op->GetPointCount();
    const CPolygon *polyAdr = op->GetPolygonR();
    int polycnt = op->GetPolygonCount();

    // get name of object as string copy (free it after usage!)
    char *pChar = op->GetName().GetCStringCopy();
    if (pChar)
    {
      printf("\n - AlienPolygonObjectData (%d): %s\n", (int)op->GetType(), pChar);
      DeleteMem(pChar);
    }
    else
      printf("\n - AlienPolygonObjectData (%d): <noname>\n", (int)op->GetType());

    printf("   - PointCount: %d PolygonCount: %d\n", (int)pointcnt, (int)polycnt);

    Matrix mtx = op->GetMg();

    // polygon object with no points/polys allowed
    if (pointcnt == 0 && polycnt == 0)
      return;

    if (!vAdr || (!polyAdr && polycnt > 0))
      return;

    // print 4 points
    for (int p = 0; p < pointcnt && p < 4; p++)
      printf("     P %d: %.1f / %.1f / %.1f\n", (int)p, vAdr[p].x, vAdr[p].y, vAdr[p].z);

    // Ngons
    int ncnt = op->GetNgonCount();
    if (ncnt > 0)
    {
      printf("\n   - %d Ngons found\n", (int)ncnt);
      for (int n=0; n<ncnt && n<3; n++) // show only 3
      {
        printf("     Ngon %d with %d Edges\n", (int)n, (int)op->GetNgonBase()->GetNgons()[n].GetCount());
        for (int p=0; p<polycnt && p<3; p++)
        {
          int polyid = op->GetNgonBase()->FindPolygon(p);
          if (polyid != NOTOK)
            printf("     Polygon %d is included in Ngon %d\n", (int)p, (int)polyid);
          else
            printf("     Polygon %d is NOT included in any Ngon\n", (int)p);
          if (p==2)
            printf("     ...\n");
        }
        for (int e=0; e<op->GetNgonBase()->GetNgons()[n].GetCount()&&e<3; e++)
        {
          PgonEdge *pEdge = op->GetNgonBase()->GetNgons()[n].GetEdge(e);
          printf("     Edge %d: eidx: %d pid: %d sta: %d f:%d e:%d\n", (int)e, (int)pEdge->EdgeIndex(), (int)pEdge->ID(), (int)pEdge->State(), (int)pEdge->IsFirst(), (int)pEdge->IsSegmentEnd());
          if (e==2)
            printf("     ...\n");
        }
        if (ncnt==2)
          printf("     ...\n");
      }
      printf("\n");
    }

    // tag info
    PrintTagInfo(op);

    // detect the assigned material (it only supports the single material of the first found texture tag, there can be more materials/texture tags assigned to an object)
    BaseTag *pTex = op->GetTag(Ttexture);
    if (pTex)
    {
      AlienMaterial *pMat = NULL;
      if (pTex->GetParameter(TEXTURETAG_MATERIAL, data))
        pMat = (AlienMaterial*)data.GetLink();
      if (pMat)
        matid = pMat->id;
    }

    // detect the assigned layer
    AlienLayer *pLay = (AlienLayer*)op->GetLayerObject();
    if (pLay)
    {
      layid=pLay->id;
      pChar = pLay->GetName().GetCStringCopy();
      if (pChar)
      {
        printf("   - Layer (%d): %s\n", (int)pLay->GetType(), pChar);
        DeleteMem(pChar);
      }
      else
        printf("   - Layer (%d): <noname>\n", (int)pLay->GetType());
    }

    if (op->GetFirstCTrack())
      PrintAnimInfo(this->GetNode());
  }

  void AlienLayer::Print()
  {
    char *pChar = GetName().GetCStringCopy();
    if (pChar)
    {
      printf("\n - AlienLayer: \"%s\"", pChar);
      DeleteMem(pChar);
    }
    else
      printf("\n - AlienLayer: <noname>");

    // access and print layer data
    GeData data;
    Bool s, m, a, g, d, e, v, r, l;
    Vector c;
    GetParameter(DescID(ID_LAYER_SOLO), data);				s = !!data.GetInt32();
    GetParameter(DescID(ID_LAYER_MANAGER), data);			m = !!data.GetInt32();
    GetParameter(DescID(ID_LAYER_ANIMATION), data);		a = !!data.GetInt32();
    GetParameter(DescID(ID_LAYER_GENERATORS), data);	g = !!data.GetInt32();
    GetParameter(DescID(ID_LAYER_DEFORMERS), data);		d = !!data.GetInt32();
    GetParameter(DescID(ID_LAYER_EXPRESSIONS), data);	e = !!data.GetInt32();
    GetParameter(DescID(ID_LAYER_VIEW), data);				v = !!data.GetInt32();
    GetParameter(DescID(ID_LAYER_RENDER), data);			r = !!data.GetInt32();
    GetParameter(DescID(ID_LAYER_LOCKED), data);			l = !!data.GetInt32();
    GetParameter(DescID(ID_LAYER_COLOR), data);				c = !!data.GetVector();
    printf(" - S%d V%d R%d M%d L%d A%d G%d D%d E%d C%d/%d/%d\n", s, v, r, m, l, a, g, d, e, (int)(c.x*255.0), (int)(c.y*255.0), (int)(c.z*255.0));

    PrintUniqueIDs(this);

    // assign a id to the layer
    id = tempLayID;

    tempLayID++;
  }

  void AlienMaterial::Print()
  {
    char *pChar = GetName().GetCStringCopy();
    if (pChar)
    {
      printf("\n - AlienMaterial (%d): %s\n", (int)GetType(), pChar);
      DeleteMem(pChar);
    }
    else
      printf("\n - AlienMaterial (%d): <noname>\n", (int)GetType());

    PrintUniqueIDs(this);

    // assign a id to the material
    id = tempmatid;
    tempmatid++;

    // material preview custom data type
    GeData mData;
    if (GetParameter(MATERIAL_PREVIEW, mData))
    {
      MaterialPreviewData *mPreview = (MaterialPreviewData*)mData.GetCustomDataType(CUSTOMDATATYPE_MATPREVIEW);
      if (mPreview)
      {
        MatPreviewType mPreviewType = mPreview->GetPreviewType();
        MatPreviewSize mPreviewSize = mPreview->GetPreviewSize();
        printf("   MaterialPreview: Type:%d Size:%d\n", mPreviewType, mPreviewSize);
      }
    }

    GeData data;
    Vector col = Vector(0.0, 0.0, 0.0);
    if (GetChannelState(CHANNEL_COLOR) && GetParameter(MATERIAL_COLOR_COLOR, data))
    {
      col = data.GetVector();
      printf(" - Color: %d / %d / %d", (int)(col.x*255), (int)(col.y*255), (int)(col.z*255));
    }
    printf("\n");


    PrintShaderInfo(GetShader(MATERIAL_COLOR_SHADER), 4);

    PrintAnimInfo(this);

  }


  void PrintUniqueIDs(BaseList2D *op)
  {
    // actual UIDs - size can be different from application to application
    Int32 t, i, cnt = op->GetUniqueIDCount();
    for (i=0; i<cnt; i++)
    {
      Int32 appid;
      Int bytes;
      const char *mem;
      if (op->GetUniqueIDIndex(i, appid, mem, bytes))
      {
        if (!op->FindUniqueID(appid, mem, bytes))
        {
          CriticalStop();
          continue;
        }
        printf("   - UniqueID (%d Byte): ", (int)bytes);
        for (t=0; t<bytes; t++)
        {
          printf("%2.2x", (UChar)mem[t]);
        }
        printf(" [AppID: %d]", (int)appid);
        printf("\n");
      }
    }

    BaseContainer *bc = op->GetDataInstance();

    // Allplan related (deprecated)
    char *pChar = bc->GetString(AllplanElementID).GetCStringCopy();
    if (pChar)
    {
      printf("   - Allplan ElementID: %s\n", pChar);
      DeleteMem(pChar);
    }
    pChar = bc->GetString(AllplanAllrightID).GetCStringCopy();
    if (pChar)
    {
      printf("   - Allplan AllrightID: %s\n", pChar);
      DeleteMem(pChar);
    }

    // ArchiCAD related (deprecated)
    pChar = bc->GetString(ID_ARCHICADNAME).GetCStringCopy();
    if (pChar)
    {
      printf("   - ArchiCAD String ID: %s\n", pChar);
      DeleteMem(pChar);
    }
  }

  void PrintUniqueIDs(NodeData *op)
  {
    if (op)
      PrintUniqueIDs(op->GetNode());
  }
  // prints tag infos to the console
  void PrintTagInfo(BaseObject *obj)
  {
    if (!obj)
      return;

    char *pChar = NULL, buffer[256];
    GeData data;

    // browse all tags and access data regarding type
    BaseTag* btag = obj->GetFirstTag();
    for (; btag; btag=(BaseTag*)btag->GetNext())
    {
      // name for all tag types
      pChar = btag->GetName().GetCStringCopy();
      if (pChar)
      {
        printf("   - %s \"%s\"", GetObjectTypeName(btag->GetType()), pChar);
        DeleteMem(pChar);
      }
      else
      {
        printf("   - %s \"\"", GetObjectTypeName(btag->GetType()));
      }

      // compositing tag
      if (btag->GetType() == Tcompositing)
      {
        if (btag->GetParameter(COMPOSITINGTAG_MATTEOBJECT, data) && data.GetInt32())
        {
          if (btag->GetParameter(COMPOSITINGTAG_MATTECOLOR, data))
            printf("     + Matte - Color R %d G %d B %d", (int)(data.GetVector().x*255.0), (int)(data.GetVector().y*255.0), (int)(data.GetVector().z*255.0));
          else
            printf("     + Matte - Color NOCOLOR");
        }
        if (btag->GetParameter(COMPOSITINGTAG_ENABLECHN4, data) && data.GetInt32())
        {
          if (btag->GetParameter(COMPOSITINGTAG_IDCHN4, data))
            printf("     + Objectbuffer Channel 5 enabled - ID = %d", (int)data.GetInt32());
        }
      }
      // phong tag
      if (btag->GetType() == Tphong)
      {
        if (btag->GetParameter(PHONGTAG_PHONG_ANGLE, data))
          printf(" - Phong Angle = %f", data.GetFloat()*180.0/PI);
      }

      // archi grass tag
      if (btag->GetType() == Tarchigrass)
      {
      }

      // target expression
      if (btag->GetType() == Ttargetexpression)
      {
        BaseList2D * obj = btag->GetData().GetLink(TARGETEXPRESSIONTAG_LINK);
        if (obj)
        {
          obj->GetName().GetCString(buffer, sizeof buffer);
          printf(" - linked to \"%s\"", buffer);
        }
      }

      // align to path
      if (btag->GetType() == Taligntopath)
      {
        if (btag->GetParameter(ALIGNTOPATHTAG_LOOKAHEAD, data))
        {
          printf(" - Look Ahead: %f", data.GetTime().Get());
        }
      }

      // vibrate
      if (btag->GetType() == Tvibrate)
      {
        if (btag->GetParameter(VIBRATEEXPRESSION_RELATIVE, data))
        {
          printf(" - Relative: %d", (int)data.GetInt32());
        }
      }

      // coffee
      if (btag->GetType() == Tcoffeeexpression)
      {
        if (btag->GetParameter(1000, data))
        {
          pChar = data.GetString().GetCStringCopy();
          printf("     Code:\n---\n%s\n---\n", pChar);
          DeleteMem(pChar);
        }
      }

      // WWW tag
      if (btag->GetType() == Twww)
      {
        if (btag->GetParameter(WWWTAG_URL, data))
        {
          pChar = data.GetString().GetCStringCopy();
          printf("     URL:  \"%s\"", pChar);
          DeleteMem(pChar);
        }
        if (btag->GetParameter(WWWTAG_INFO, data))
        {
          pChar = data.GetString().GetCStringCopy();
          printf("     INFO: \"%s\"", pChar);
          DeleteMem(pChar);
        }
      }

      // weight tag
      if (btag->GetType() == Tweights)
      {
        WeightTagData *wt = (WeightTagData*)btag->GetNodeData();
        if (wt)
        {
          int pCnt = 0;
          int jCnt = wt->GetJointCount();
          printf(" - Joint Count: %d\n", (int)jCnt);

          BaseObject *jOp = NULL;
          // print data for 3 joints and 3 points only
          for (int j=0; j<jCnt && j<3; j++)
          {
            jOp = wt->GetJoint(j, obj->GetDocument());
            if (jOp)
            {
              pChar = jOp->GetName().GetCStringCopy();
              printf("     Joint %d: \"%s\"\n", (int)j, pChar);
              DeleteMem(pChar);
            }
            printf("     Joint Weight Count:  %d\n", (int)wt->GetWeightCount(j));
            if (obj->GetType() == Opolygon)
            {
              pCnt = ((PolygonObject*)obj)->GetPointCount();
              for (int p=0; p<pCnt && p<3; p++)
              {
                printf("     Weight at Point %d: %f\n", (int)p, wt->GetWeight(j, p));
              }
              if (pCnt>=3)
                printf("     ...\n");
            }
          }
          if (jCnt>=3 && pCnt<3)
            printf("     ...\n");
        }
      }

      // texture tag
      if (btag->GetType() == Ttexture)
      {
        // detect the material
        AlienMaterial *mat = NULL;
        if (btag->GetParameter(TEXTURETAG_MATERIAL, data))
          mat = (AlienMaterial*)data.GetLink();
        if (mat)
        {
          char *pCharMat=NULL, *pCharSh=NULL;
          Vector col = Vector(0.0, 0.0, 0.0);
          if (mat->GetParameter(MATERIAL_COLOR_COLOR, data))
            col = data.GetVector();

          pCharMat = mat->GetName().GetCStringCopy();
          if (pCharMat)
          {
            printf(" - material: \"%s\" (%d/%d/%d)", pCharMat, int(col.x*255), int(col.y*255), int(col.z*255));
            DeleteMem(pCharMat);
          }
          else
            printf(" - material: <noname> (%d/%d/%d)", int(col.x*255), int(col.y*255), int(col.z*255));
          // detect the shader
          BaseShader* sh=mat->GetShader(MATERIAL_COLOR_SHADER);
          if (sh)
          {
            pCharSh = sh->GetName().GetCStringCopy();
            if (pCharSh)
            {
              printf(" - color shader \"%s\" - Type: %s", pCharSh, GetObjectTypeName(sh->GetType()));
              DeleteMem(pCharSh);
            }
            else
              printf(" - color shader <noname> - Type: %s", GetObjectTypeName(sh->GetType()));
          }
          else
            printf(" - no shader");
        }
        else
          printf(" - no material");
      }

      // normal tag
      if (btag->GetType() == Tnormal)
      {
        printf("\n");
        int count = ((NormalTag*)btag)->GetDataCount();
        const CPolygon *pAdr = NULL;
        if (obj->GetType() == Opolygon)
          pAdr = ((PolygonObject*)obj)->GetPolygonR();
        // print normals of 2 polys
        for (int n=0; n<count && n<2; n++)
        {
          // MAGNUS: broken
/*
          NormalStruct pNormals = ((NormalTag*)btag)->GetNormals(n);
          printf("     Na %d: %.6f / %.6f / %.6f\n", (int)n, pNormals.a.x, pNormals.a.y, pNormals.a.z);
          printf("     Nb %d: %.6f / %.6f / %.6f\n", (int)n, pNormals.b.x, pNormals.b.y, pNormals.b.z);
          printf("     Nc %d: %.6f / %.6f / %.6f\n", (int)n, pNormals.c.x, pNormals.c.y, pNormals.c.z);
          if (!pAdr || pAdr[n].c != pAdr[n].d)
            printf("     Nd %d: %.6f / %.6f / %.6f\n", (int)n, pNormals.d.x, pNormals.d.y, pNormals.d.z);
*/
        }
      }

      // UVW tag
      if (btag->GetType() == Tuvw)
      {
        printf("\n");
        UVWStruct uvw;
        int uvwCount = ((UVWTag*)btag)->GetDataCount();
        // print for 4 polys uvw infos
        for (int u=0; u<uvwCount && u<4; u++)
        {
          ((UVWTag*)btag)->Get(((UVWTag*)btag)->GetDataAddressR(), u, uvw);
          printf("     Poly %d: %.2f %.2f %.2f / %.2f %.2f %.2f / %.2f %.2f %.2f / %.2f %.2f %.2f \n", (int)u, uvw.a.x, uvw.a.y, uvw.a.z, uvw.b.x, uvw.b.y, uvw.b.z, uvw.c.x, uvw.c.y, uvw.c.z, uvw.d.x, uvw.d.y, uvw.d.z);
        }
      }

      // Polygon Selection Tag
      if (btag->GetType() == Tpolygonselection && obj->GetType() == Opolygon)
      {
        printf("\n");
        BaseSelect *bs = ((SelectionTag*)btag)->GetBaseSelect();
        if (bs)
        {
          int s = 0;
          for (s=0; s<((PolygonObject*)obj)->GetPolygonCount() && s<5; s++)
          {
            if (bs->IsSelected(s))
              printf("     Poly %d: selected\n", (int)s);
            else
              printf("     Poly %d: NOT selected\n", (int)s);
          }
          if (s < ((PolygonObject*)obj)->GetPolygonCount())
            printf("     ...\n");
        }
      }

      printf("\n");
    }
  }

  // shows how to access parameters of 3 different shader types and prints it to the console
  void PrintShaderInfo(BaseShader *shader, int depth)
  {
    BaseShader *sh = shader;
    while (sh)
    {
      for (int s=0; s<depth; s++) printf(" ");

      // type layer shader
      if (sh->GetType() == Xlayer)
      {
        printf("LayerShader - %d\n", (int)sh->GetType());

        BaseContainer* pData = sh->GetDataInstance();
        GeData blendData = pData->GetData(SLA_LAYER_BLEND);
        iBlendDataType* d = (iBlendDataType*)blendData.GetCustomDataType(CUSTOMDATA_BLEND_LIST);
        if (d)
        {
          LayerShaderLayer *lsl = (LayerShaderLayer*)(d->m_BlendLayers.GetObject(0));

          while (lsl)
          {
            for (int s=0; s<depth; s++) printf(" ");

            printf(" LayerShaderLayer - %s (%d)\n", GetObjectTypeName(lsl->GetType()), lsl->GetType());

            // folder ?
            if (lsl->GetType() == TypeFolder)
            {
              LayerShaderLayer *subLsl = (LayerShaderLayer*)((BlendFolder*)lsl)->m_Children.GetObject(0);
              while (subLsl)
              {
                for (int s=0; s<depth; s++) printf(" ");
                printf("  Shader - %s (%d)\n", GetObjectTypeName(subLsl->GetType()), subLsl->GetType());

                // base shader ?
                if (subLsl->GetType() == TypeShader)
                  PrintShaderInfo((BaseShader*)((BlendShader*)subLsl)->m_pLink->GetLink(), depth+1);

                subLsl = subLsl->GetNext();
              }
            }
            else if (lsl->GetType() == TypeShader)
              PrintShaderInfo((BaseShader*)((BlendShader*)lsl)->m_pLink->GetLink(), depth+2);

            lsl = lsl->GetNext();
          }
        }
      }
      // type bitmap shader (texture)
      else
      {
        if (sh->GetType() == Xbitmap)
        {
          char *pCharShader =  sh->GetFileName().GetString().GetCStringCopy();
          if (pCharShader)
          {
            printf("Shader - %s (%d) : %s\n", GetObjectTypeName(sh->GetType()), (int)sh->GetType(), pCharShader);
            DeleteMem(pCharShader);
            pCharShader =  sh->GetFileName().GetFileString().GetCStringCopy();
            if (pCharShader)
            {
              for (int s=0; s<depth; s++) printf(" ");
              printf("texture name only: \"%s\"\n", pCharShader);
              DeleteMem(pCharShader);
            }
          }
          else
          {
            printf("Shader - %s (%d) : ""\n", GetObjectTypeName(sh->GetType()), (int)sh->GetType());
          }
        }
        // type gradient shader
        else if (sh->GetType() == Xgradient)
        {
          printf("Shader - %s (%d) : ", GetObjectTypeName(sh->GetType()), (int)sh->GetType());
          GeData data;
          sh->GetParameter(SLA_GRADIENT_GRADIENT, data);
          Gradient *pGrad = (Gradient*)data.GetCustomDataType(CUSTOMDATATYPE_GRADIENT);
          int kcnt = pGrad->GetKnotCount();
          printf(" %d Knots\n", (int)kcnt);
          for (int k=0; k<kcnt; k++)
          {
            GradientKnot kn = pGrad->GetKnot(k);
            for (int s=0; s<depth; s++) printf(" ");
            printf("   -> %d. Knot: %.1f/%.1f/%.1f\n", (int)k, kn.col.x*255.0, kn.col.y*255.0, kn.col.z*255.0);
          }
        }
        else
        {
          printf("Shader - %s (%d)\n", GetObjectTypeName(sh->GetType()), (int)sh->GetType());

          PrintShaderInfo(sh->GetDown(), depth+1);
        }
      }

      sh = sh->GetNext();
    }
  }

  // shows how to access render data parameter and prints it to the console
  void PrintRenderDataInfo(RenderData *rdata)
  {
    if (!rdata)
      return;

    printf("\n\n # Render Data #\n");

    GeData data;
    // renderer
    if (rdata->GetParameter(RDATA_RENDERENGINE, data))
    {
      switch (data.GetInt32())
      {
        case RDATA_RENDERENGINE_PREVIEWSOFTWARE:
          printf(" - Renderengine - PREVIEWSOFTWARE\n");
          break;

        case RDATA_RENDERENGINE_PREVIEWHARDWARE:
          printf(" - Renderengine - PREVIEWHARDWARE\n");
          break;

        case RDATA_RENDERENGINE_CINEMAN:
          printf(" - Renderengine - CINEMAN\n");
          break;

        default:
          printf(" - Renderengine - STANDARD\n");
      }
    }

    // save option on/off ?
    if (rdata->GetParameter(RDATA_GLOBALSAVE, data) && data.GetInt32())
    {
      printf(" - Global Save - ENABLED\n");
      if (rdata->GetParameter(RDATA_SAVEIMAGE, data) && data.GetInt32())
      {
        if (rdata->GetParameter(RDATA_PATH, data))
        {
          char *pChar = data.GetFilename().GetString().GetCStringCopy();
          if (pChar)
          {
            printf("   + Save Image - %s\n", pChar);
            DeleteMem(pChar);
          }
          else
            printf("   + Save Image\n");
        }
        else
          printf("   + Save Image\n");
      }
      // save options: alpha, straight alpha, separate alpha, dithering, sound
      if (rdata->GetParameter(RDATA_ALPHACHANNEL, data) && data.GetInt32())
        printf("   + Alpha Channel\n");
      if (rdata->GetParameter(RDATA_STRAIGHTALPHA, data) && data.GetInt32())
        printf("   + Straight Alpha\n");
      if (rdata->GetParameter(RDATA_SEPARATEALPHA, data) && data.GetInt32())
        printf("   + Separate Alpha\n");
      if (rdata->GetParameter(RDATA_TRUECOLORDITHERING, data) && data.GetInt32())
        printf("   + 24 Bit Dithering\n");
      if (rdata->GetParameter(RDATA_INCLUDESOUND, data) && data.GetInt32())
        printf("   + Include Sound\n");
    }
    else
      printf(" - Global Save = FALSE\n");

    // multi pass enabled ?
    if (rdata->GetParameter(RDATA_MULTIPASS_ENABLE, data) && data.GetInt32())
    {
      printf(" - Multi pass - ENABLED\n");
      if (rdata->GetParameter(RDATA_MULTIPASS_SAVEIMAGE, data) && data.GetInt32())
      {
        if (rdata->GetParameter(RDATA_MULTIPASS_FILENAME, data))
        {
          char *pChar = data.GetFilename().GetString().GetCStringCopy();
          if (pChar)
          {
            printf("   + Save Multi pass Image - %s\n", pChar);
            DeleteMem(pChar);
          }
          else
            printf("   + Save Multi pass Image\n");
        }
        else
          printf("   + Save Multi pass Image\n");
      }

      if (rdata->GetParameter(RDATA_MULTIPASS_STRAIGHTALPHA, data) && data.GetInt32())
        printf("   + Multi pass Straight Alpha\n");
      MultipassObject *mobj = rdata->GetFirstMultipass();
      GeData data;
      if (mobj)
      {
        while (mobj)
        {
          if (mobj->GetParameter(MULTIPASSOBJECT_TYPE, data))
          {
            printf("   + Multi pass Channel: %d", (int)data.GetInt32());
            if (data.GetInt32() == VPBUFFER_OBJECTBUFFER)
            {
              if (mobj->GetParameter(MULTIPASSOBJECT_OBJECTBUFFER, data))
                printf(" Group ID %d", (int)data.GetInt32());
            }

            printf("\n");
            mobj = (MultipassObject*)mobj->GetNext();
          }
        }
      }
    }
    // print out enabled post effects
    BaseVideoPost *vp = rdata->GetFirstVideoPost();
    if (vp)
      printf(" - VideoPostEffects:\n");
    while (vp)
    {
      // enabled / disabled ?
      printf("   + %s ", vp->GetBit(BIT_VPDISABLED) ? "[OFF]" : "[ON ]");

      switch (vp->GetType())
      {
        case VPambientocclusion:
          printf("Ambient Occlusion");
          if (vp->GetParameter(VPAMBIENTOCCLUSION_ACCURACY, data) && data.GetType() == DA_REAL)
            printf(" (Accuracy = %f)", (100.0 * data.GetFloat()));
          break;
        case VPcelrender:
          printf("Celrender");
          if (vp->GetParameter(VP_COMICOUTLINE, data) && data.GetType() == DA_LONG)
            printf(" (Outline = %s)", data.GetInt32() ? "TRUE" : "FALSE");
          break;
        case VPcolorcorrection:
          printf("Colorcorrection");
          if (vp->GetParameter(ID_PV_FILTER_CONTRAST, data) && data.GetType() == DA_REAL)
            printf(" (Contrast = %f)", (100.0 * data.GetFloat()));
          break;
        case VPcolormapping:
          printf("Colormapping");
          if (vp->GetParameter(COLORMAPPING_BACKGROUND, data) && data.GetType() == DA_LONG)
            printf(" (Affect Background = %s)", data.GetInt32() ? "TRUE" : "FALSE");
          break;
        case VPcylindricallens:
          printf("Cylindrical Lens");
          if (vp->GetParameter(CYLINDERLENS_VERTICALSIZE, data) && data.GetType() == DA_REAL)
            printf(" (Vertical Size = %f)", data.GetFloat());
          break;
        case VPdepthoffield:
          printf("Depth of Field");
          if (vp->GetParameter(DB_DBLUR, data) && data.GetType() == DA_REAL)
            printf(" (Distance Blur = %f)", 100*data.GetFloat());
          break;
        case VPglobalillumination:
          printf("Global Illumination");
          if (vp->GetParameter(GI_SETUP_DATA_EXTRA_REFLECTIVECAUSTICS, data) && data.GetType() == DA_LONG)
            printf(" (Reflective Caustics = %s)", data.GetInt32() ? "TRUE" : "FALSE");
          break;
        case VPglow:
          printf("Glow");
          if (vp->GetParameter(GW_LUM, data) && data.GetType() == DA_REAL)
            printf(" (Luminosity = %f)", 100*data.GetFloat());
          break;
        case VPhair:
          printf("Hair");
          if (vp->GetParameter(HAIR_RENDER_SHADOW_DIST_ACCURACY, data) && data.GetType() == DA_REAL)
            printf(" (Depth Threshold = %f)", 100*data.GetFloat());
          break;
        case VPhighlights:
          printf("Highlights");
          if (vp->GetParameter(HLIGHT_SIZE, data) && data.GetType() == DA_REAL)
            printf(" (Flare Size = %f)", 100*data.GetFloat());
          break;
        case VPlenseffects:
          printf("Lenseffects");
          break;
        case VPmedianfilter:
          printf("Medianfilter");
          if (vp->GetParameter(VP_MEDIANFILTERSTRENGTH, data) && data.GetType() == DA_REAL)
            printf(" (Strength = %f)", 100*data.GetFloat());
          break;
        case VPobjectglow:
          printf("Objectglow");
          break;
        case VPobjectmotionblur:
          printf("Objectmotionblur");
          if (vp->GetParameter(VP_OMBSTRENGTH, data) && data.GetType() == DA_REAL)
            printf(" (Strength = %f)", 100*data.GetFloat());
          break;
        case VPsharpenfilter:
          printf("Sharpenfilter");
          if (vp->GetParameter(VP_SHARPENFILTERSTRENGTH, data) && data.GetType() == DA_REAL)
            printf(" (Strength = %f)", 100*data.GetFloat());
          break;
        case VPscenemotionblur:
          printf("Scenemotionblur");
          if (vp->GetParameter(VP_SMBDITHER, data) && data.GetType() == DA_REAL)
            printf(" (Dithering = %f)", 100*data.GetFloat());
          vp->SetParameter(VP_SMBDITHER, 12.3*0.01); // set test
          break;
        case VPremote:
          printf("Remote");
          if (vp->GetParameter(VP_REMOTEPATH, data) && data.GetType() == DA_FILENAME)
          {
            char *tmp = data.GetFilename().GetString().GetCStringCopy();
            printf(" (Ext. Appl. = \'%s\')", tmp);
            DeleteMem(tmp);
          }
          break;
        case VPsketch:
          printf("Sketch & Toon");
          if (vp->GetParameter(OUTLINEMAT_LINE_INTERSECTION, data) && data.GetType() == DA_LONG)
            printf(" (Intersections = %s)", data.GetInt32() ? "TRUE" : "FALSE");
          break;
        case VPsoftenfilter:
          printf("Softenfilter");
          if (vp->GetParameter(VP_SOFTFILTERSTRENGTH, data) && data.GetType() == DA_REAL)
            printf(" (Strength = %f)", 100*data.GetFloat());
          break;
        case VPvectormotionblur:
          printf("Vectormotionblur");
          if (vp->GetParameter(MBLUR_SAMPLES, data) && data.GetType() == DA_LONG)
            printf(" (Samples = %d)", (int)data.GetInt32());
          break;
      }
      printf("\n");

      vp = (BaseVideoPost*)vp->GetNext();
    }

    printf("\n");
  }

  // prints animation track and key infos to the console
  void PrintAnimInfo(BaseList2D *bl)
  {
    if (!bl || !bl->GetFirstCTrack())
      return;

    printf("\n   # Animation Info #");

    int tn = 0;
    CTrack *ct = bl->GetFirstCTrack();
    while (ct)
    {
      // CTrack name
      char *pChar = ct->GetName().GetCStringCopy();
      if (pChar)
      {
        printf("\n   %d. CTrack \"%s\"!\n", (int)++tn, pChar);
        DeleteMem(pChar);
      }
      else
        printf("\n   %d. CTrack !\n", (int)++tn);

      // time track
      CTrack *tt = ct->GetTimeTrack(bl->GetDocument());
      if (tt)
      {
        printf("\n   -> has TimeTrack !\n");
        CCurve *tcc = ct->GetCurve();
        if (tcc)
        {
          printf("    Has CCurve with %d Keys\n", (int)tcc->GetKeyCount());

          CKey *ck = NULL;
          BaseTime t;
          for (int k=0; k<tcc->GetKeyCount(); k++)
          {
            ck = tcc->GetKey(k);
            t = ck->GetTime();
            printf("     %d. Key : %d - %f\n", (int)k+1, (int)t.GetFrame(25), ck->GetValue());
          }
        }
      }

      // get DescLevel id
      DescID testID = ct->GetDescriptionID();
      DescLevel lv = testID[0];
      ct->SetDescriptionID(ct, testID);
      printf("   DescID->DescLevel->ID: %d\n", (int)lv.id);

      // CTrack type
      switch (ct->GetTrackCategory())
      {
        case PSEUDO_VALUE:
          printf("   VALUE - Track found!\n");
          break;

        case PSEUDO_DATA:
          printf("   DATA - Track found!\n");
          break;

        case PSEUDO_PLUGIN:
          if (ct->GetType() == CTpla)
            printf("   PLA - Track found!\n");
          else if (ct->GetType() == CTdynamicspline)
            printf("   Dynamic Spline Data - Track found!\n");
          else if (ct->GetType() == CTmorph)
            printf("   MORPH - Track found!\n");
          else
            printf("   unknown PLUGIN - Track found!\n");
          break;

        case PSEUDO_UNDEF:
        default:
          printf("   UNDEFINDED - Track found!\n");
      }

      // get CCurve and print key frame data
      CCurve *cc = ct->GetCurve();
      if (cc)
      {
        printf("   Has CCurve with %d Keys\n", (int)cc->GetKeyCount());

        CKey *ck = NULL;
        BaseTime t;
        for (int k=0; k<cc->GetKeyCount(); k++)
        {
          ck = cc->GetKey(k);

          t = ck->GetTime();
          if (ct->GetTrackCategory()==PSEUDO_VALUE)
          {
            printf("    %d. Key : %d - %f\n", (int)k+1, (int)t.GetFrame(25), ck->GetValue());
          }
          else if (ct->GetTrackCategory()==PSEUDO_PLUGIN && ct->GetType() == CTpla)
          {
            GeData ptData;
            printf("    %d. Key : %d - ", (int)k+1, (int)t.GetFrame(25));

            // bias
            if (ck->GetParameter(CK_PLA_BIAS, ptData) && ptData.GetType() == DA_REAL)
              printf("Bias = %.2f - ", ptData.GetFloat());

            // smooth
            if (ck->GetParameter(CK_PLA_CUBIC, ptData) && ptData.GetType() == DA_LONG)
              printf("Smooth = %d - ", (int)ptData.GetInt32());

            // pla data
            if (ck->GetParameter(CK_PLA_DATA, ptData))
            {
              PLAData *plaData = (PLAData*)ptData.GetCustomDataType(CUSTOMDATATYPE_PLA);
              PointTag *poiTag;
              TangentTag *tanTag;
              plaData->GetVariableTags(poiTag, tanTag);
              if (poiTag && poiTag->GetCount() > 0)
              {
                Vector *a = poiTag->GetPointAdr();
                // print values for first point only
                printf("%.3f / %.3f / %.3f", a[0].x, a[0].y, a[0].z);
              }
              else
                printf("no points?");
            }

            printf("\n");
          }
          else if (ct->GetTrackCategory()==PSEUDO_PLUGIN && ct->GetType() == CTmorph)
          {
            GeData mtData;
            printf("    %d. Key : %d - ", (int)k+1, (int)t.GetFrame(25));

            // bias
            if (ck->GetParameter(CK_MORPH_BIAS, mtData) && mtData.GetType() == DA_REAL)
              printf("Bias = %.2f - ", mtData.GetFloat());

            // smooth
            if (ck->GetParameter(CK_MORPH_CUBIC, mtData) && mtData.GetType() == DA_LONG)
              printf("Smooth = %d - ", (int)mtData.GetInt32());

            // link to target object
            if (ck->GetParameter(CK_MORPH_LINK, mtData))
            {
              BaseObject *targetObject = (BaseObject*)mtData.GetLink();
              if (targetObject)
              {
                char *pTargetChar = targetObject->GetName().GetCStringCopy();
                if (pTargetChar)
                {
                  printf("Target Object = %s", pTargetChar);
                  DeleteMem(pTargetChar);
                }
                else
                  printf("no target object name");
              }
              else
                printf("no target object defined...");
            }

            printf("\n");
          }
        } // for

      }
      ct = ct->GetNext();
    }
    printf("\n");
  }

  // print matrix data to the console
  void PrintMatrix(Matrix m)
  {
    printf("   - Matrix:");
    int size = 6;
    Float f = m.v1.x;
    if (f==0.0)f=0.0;
    if (f<0.0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f", f);
    size = 6;
    f = m.v1.y;
    if (f==0.0)f=0.0;
    if (f<0.0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f", f);
    size = 6;
    f = m.v1.z;
    if (f==0.0)f=0.0;
    if (f<0.0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f\n", f);

    printf("           :");
    size = 6;
    f = m.v2.x;
    if (f==0.0)f=0.0;
    if (f<0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f", f);
    size = 6;
    f = m.v2.y;
    if (f==0.0)f=0.0;
    if (f<0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f", f);
    size = 6;
    f = m.v2.z;
    if (f==0.0)f=0.0;
    if (f<0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f\n", f);

    printf("           :");
    size = 6;
    f = m.v3.x;
    if (f==0.0)f=0.0;
    if (f<0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f", f);
    size = 6;
    f = m.v3.y;
    if (f==0.0)f=0.0;
    if (f<0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f", f);
    size = 6;
    f = m.v3.z;
    if (f==0.0)f=0.0;
    if (f<0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f\n", f);

    printf("           :");
    size = 6;
    f = m.off.x;
    if (f==0.0)f=0.0;
    if (f<0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f", f);
    size = 6;
    f = m.off.y;
    if (f==0.0)f=0.0;
    if (f<0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f", f);
    size = 6;
    f = m.off.z;
    if (f==0.0)f=0.0;
    if (f<0)size--; if (f>=10.0||f<=-10.0)size--; if (f>=100.0||f<=-100.0)size--; if (f>=1000.0||f<=-1000.0)size--; if (f>=10000.0||f<=-10000.0)size--;
    for (int s=0; s<size; s++) printf(" ");
    printf("%f\n", f);
  }


}

