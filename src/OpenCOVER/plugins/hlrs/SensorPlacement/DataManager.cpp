#include<osg/Material>
#include<osg/LightModel>
#include<osg/StateSet>
#include <cover/coVRPluginSupport.h>

#include <algorithm>

#include "DataManager.h"
#include "Zone.h"
#include "Sensor.h"

void setStateSet(osg::StateSet *stateSet)
{
    osg::Material *material = new osg::Material();
    material->setColorMode(osg::Material::AMBIENT_AND_DIFFUSE); 
    stateSet->setAttributeAndModes(material, osg::StateAttribute::ON);
}

std::vector<VisibilityMatrix<float>> convertVisMatTo2D(const VisibilityMatrix<float>& visMat)
{
    std::vector<int> sizes;
    for(const auto& zone : DataManager::GetSafetyZones())
    {
        sizes.push_back(zone->getNumberOfPoints());
    } 
    
    std::vector<VisibilityMatrix<float>> result;
    result.reserve(sizes.size());
    
    size_t startPos{0};
    size_t endPos;
    for(const auto& size : sizes)
    {
        endPos = startPos + size;
        VisibilityMatrix<float> temp = {visMat.begin() + startPos,visMat.begin() + endPos};
        result.push_back(std::move(temp));
        startPos += size;
    }
    
    return result;
}

DataManager::DataManager()
{
    m_Root = new osg::Group();
    m_Root->setName("SensorPlacement");
    m_Root->setNodeMask(m_Root->getNodeMask() & ~ 4096);
    osg::StateSet *mystateset = m_Root->getOrCreateStateSet();
    setStateSet(mystateset);
    cover->getObjectsRoot()->addChild(m_Root.get());
    std::cout<<"Singleton DataManager created!"<<std::endl;
};

void DataManager::preFrame()
{
    for(const auto& zone : GetInstance().m_SafetyZones)
    {
        bool status = zone->preFrame();
        if(!status)
        {
            GetInstance().RemoveZone(zone.get());
            return;
        }
        
    }

    for(const auto& zone : GetInstance().m_SensorZones)
    {
        bool status = zone->preFrame();
        if(!status)
        {
            GetInstance().RemoveZone(zone.get());
            return;
        } 
      
    }

    for(const auto& sensor : GetInstance().m_Sensors)
    {
        bool status = sensor->preFrame();
        if(!status)
        {
            GetInstance().RemoveSensor(sensor.get());
            return;
        }
    }

    for(const auto& udpSensor : GetInstance().m_UDPSensors)
    {
        bool status = udpSensor->preFrame();
    }
};
void DataManager::Destroy()
{
    GetInstance().m_Root->getParent(0)->removeChild(GetInstance().m_Root.get());

};

const std::vector<osg::Vec3> DataManager::GetWorldPosOfObervationPoints()
{
    std::vector<osg::Vec3> allPoints;
    size_t reserve_size {0};
    
    for(const auto& i : GetInstance().m_SafetyZones)
        reserve_size += i->getNumberOfPoints();

    allPoints.reserve(reserve_size);

    for(const auto& points :  GetInstance().m_SafetyZones)
    {
        auto vecWorldPositions = points->getWorldPositionOfPoints();
        allPoints.insert(allPoints.end(),vecWorldPositions.begin(),vecWorldPositions.end());
    }

    return allPoints;
}

void DataManager::AddZone(upZone zone)
{
    GetInstance().m_Root->addChild(zone.get()->getZone().get());

    if(dynamic_cast<SafetyZone*>(zone.get()))
        GetInstance().m_SafetyZones.push_back(std::move(zone));  
}

void DataManager::AddSensorZone(upSensorZone zone)
{
    GetInstance().m_Root->addChild(zone.get()->getZone().get());
    GetInstance().m_SensorZones.push_back(std::move(zone));  
}

void DataManager::AddSensor(upSensor sensor)
{
    GetInstance().m_Root->addChild(sensor.get()->getSensor().get());
    GetInstance().m_Sensors.push_back(std::move(sensor));     
}

void DataManager::AddUDPSensor(upSensor sensor)
{
    GetInstance().m_Root->addChild(sensor.get()->getSensor().get());
    GetInstance().m_UDPSensors.push_back(std::move(sensor));     
}

void DataManager::RemoveUDPSensor(int pos)
{
    GetInstance().m_Root->removeChild(GetInstance().m_UDPSensors.at(pos)->getSensor());
    GetInstance().m_UDPSensors.erase(GetInstance().m_UDPSensors.begin() + pos);
}

void DataManager::RemoveSensor(SensorPosition* sensor)
{  
    GetInstance().m_Root->removeChild(sensor->getSensor());

    GetInstance().m_Sensors.erase(std::remove_if(GetInstance().m_Sensors.begin(),GetInstance().m_Sensors.end(),[sensor](std::unique_ptr<SensorPosition>const& it){return sensor == it.get();}));  
}

void DataManager::RemoveZone(Zone* zone)
{
    GetInstance().m_Root->removeChild(zone->getZone());

    if(dynamic_cast<SensorZone*>(zone))
         GetInstance().m_SensorZones.erase(std::remove_if(GetInstance().m_SensorZones.begin(),GetInstance().m_SensorZones.end(),[zone](std::unique_ptr<SensorZone>const& it){return zone == it.get();}));
    else if(dynamic_cast<SafetyZone*>(zone))
        GetInstance().m_SafetyZones.erase(std::remove_if(GetInstance().m_SafetyZones.begin(),GetInstance().m_SafetyZones.end(),[zone](std::unique_ptr<Zone>const& it){return zone == it.get();}));

}

void DataManager::highlitePoints(const VisibilityMatrix<float>& visMat)
{
   auto visMat2D = convertVisMatTo2D(visMat);

    size_t count{0};
    for(const auto& zone : GetSafetyZones())
    {
        zone->highlitePoints(visMat2D.at(count));
        count ++;
    }

}

void DataManager::updateUDPSensorPosition(int pos, const osg::Matrix& mat)
{
    GetInstance().m_UDPSensors.at(pos)->setMatrix(mat);
};

void DataManager::UpdateZone(int pos, const osg::Matrix& mat)
{
    GetInstance().m_SafetyZones.at(pos)->setPosition(mat);
}; 



void DataManager::setOriginalPointColor()
{
    for(const auto& zone : GetSafetyZones())
        zone->setOriginalColor();
}


